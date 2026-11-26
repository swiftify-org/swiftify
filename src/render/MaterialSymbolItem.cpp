/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "render/MaterialSymbolItem.h"

#include "app/Fonts.h"
#include "app/Settings.h"

#include <QFont>
#include <QGlyphRun>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QQuickWindow>
#include <QRawFont>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>

namespace swiftify {

/// a name filled at one size, for one screen's pixels
struct SymbolGlyph {
    /// what a window's atlas knows it by
    QString key;
    /// how much of each device pixel the glyph covers; null when it draws nothing
    QImage coverage;
    /// the coverage's top left from the line's left end on its baseline, in
    /// device pixels
    QPoint offset;
    /// in points: the line's advance, its ascent, and its height as a Text
    /// lays it out
    qreal advance = 0;
    qreal ascent = 0;
    qreal lineHeight = 0;
    qreal devicePixelRatio = 1;
};

namespace {

/// the size names are shaped at. a glyph's index is the same at every size,
/// so one shaping serves them all
constexpr int kShapingPixels = 64;
/// an atlas page's side, in texels; it holds four planes of coverage. a
/// glyph bigger than one has a page to itself
constexpr int kPageSide = 512;
/// empty texels round every glyph, so a scaled icon's filtering never reaches
/// its neighbour's
constexpr int kPadding = 1;

int opticalSize(qreal iconSize)
{
    return iconSize < 22 ? 20 : (iconSize < 32 ? 24 : (iconSize < 44 ? 40 : 48));
}

QFont symbolFont(int pixels, int fill, int weight, qreal grade, int opsz)
{
    QFont font(fonts::iconFamily());
    font.setPixelSize(pixels);
    font.setVariableAxis(QFont::Tag("FILL"), float(fill));
    font.setVariableAxis(QFont::Tag("wght"), float(weight));
    font.setVariableAxis(QFont::Tag("GRAD"), float(grade));
    font.setVariableAxis(QFont::Tag("opsz"), float(opsz));
    return font;
}

/// a name's glyphs in the icon font, shaped with its axes; empty when the
/// font does not draw the name itself (it is not installed)
const QList<quint32> &glyphsFor(const QString &name, int fill, int weight, qreal grade, int opsz)
{
    // shaping is what copies the font's tables into a font engine of its own
    // (harfbuzz's face), and a Text shaped at every size it was shown at
    static QHash<QString, QList<quint32>> shaped;
    const QString key = QStringLiteral("%1|%2|%3|%4|%5").arg(name).arg(fill).arg(weight).arg(grade).arg(opsz);
    if (const auto found = shaped.constFind(key); found != shaped.cend()) {
        return *found;
    }
    QTextLayout layout(name, symbolFont(kShapingPixels, fill, weight, grade, opsz));
    layout.setCacheEnabled(false);
    layout.beginLayout();
    layout.createLine();
    layout.endLayout();
    const QList<QGlyphRun> runs = layout.glyphRuns();
    QList<quint32> glyphs;
    if (runs.size() == 1 && runs.first().rawFont().familyName() == fonts::iconFamily()) {
        glyphs = runs.first().glyphIndexes();
    }
    return *shaped.insert(key, glyphs);
}

/// a name drawn at a size, shared by every icon showing it so. its glyphs
/// are drawn by the platform's rasteriser through the font engine a Text of
/// that size draws with, so they come out as a Text's did
std::shared_ptr<const SymbolGlyph> glyphFor(const QString &name, qreal fill, int weight, qreal grade, qreal iconSize,
                                            qreal ratio)
{
    static QHash<QString, std::weak_ptr<const SymbolGlyph>> glyphs;
    const int filled = fill >= 0.5 ? 1 : 0;
    const int opsz = opticalSize(iconSize);
    // a Text's pixel size is a whole one
    const int pixels = std::max(1, qRound(iconSize));
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7").arg(name).arg(filled).arg(weight).arg(grade).arg(opsz)
                            .arg(pixels).arg(ratio);
    if (std::shared_ptr<const SymbolGlyph> shown = glyphs.value(key).lock()) {
        return shown;
    }

    auto glyph = std::make_shared<SymbolGlyph>();
    glyph->key = key;
    glyph->devicePixelRatio = ratio;
    const QFont font = symbolFont(pixels, filled, weight, grade, opsz);
    const QList<quint32> &indexes = glyphsFor(name, filled, weight, grade, opsz);

    // the line, and how to draw it, from its left end on the baseline
    std::function<void(QPainter &)> draw;
    QRectF bounds;
    std::optional<QTextLayout> fallback;
    if (!indexes.isEmpty()) {
        const QRawFont raw = QRawFont::fromFont(font);
        const QList<QPointF> advances = raw.advancesForGlyphIndexes(indexes);
        QList<QPointF> positions;
        qreal x = 0;
        for (qsizetype index = 0; index < indexes.size(); ++index) {
            positions.append(QPointF(x, 0));
            bounds |= raw.boundingRect(indexes.at(index)).translated(x, 0);
            x += advances.value(index).x();
        }
        glyph->advance = x;
        glyph->ascent = raw.ascent();
        glyph->lineHeight = std::ceil(raw.ascent() + raw.descent());
        QGlyphRun run;
        run.setRawFont(raw);
        run.setGlyphIndexes(indexes);
        run.setPositions(positions);
        draw = [run](QPainter &painter) { painter.drawGlyphRun(QPointF(), run); };
    } else {
        // the name as text, as a Text showed it without the icon font
        fallback.emplace(name, font);
        fallback->beginLayout();
        const QTextLine line = fallback->createLine();
        fallback->endLayout();
        if (line.isValid()) {
            glyph->advance = line.naturalTextWidth();
            glyph->ascent = line.ascent();
            glyph->lineHeight = line.height();
            bounds = QRectF(0, -line.ascent(), line.naturalTextWidth(), line.height());
            draw = [&fallback, ascent = line.ascent()](QPainter &painter) {
                fallback->draw(&painter, QPointF(0, -ascent));
            };
        }
    }

    // a little past the glyphs' bounds, for what antialiasing reaches
    const QRect area = QRectF(bounds.topLeft() * ratio, bounds.size() * ratio).toAlignedRect().adjusted(-2, -2, 2, 2);
    if (draw && !bounds.isEmpty()) {
        QImage canvas(area.size(), QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter painter(&canvas);
        painter.translate(-area.topLeft());
        painter.scale(ratio, ratio);
        painter.setPen(Qt::black);
        draw(painter);
        painter.end();
        // just what was drawn
        QRect drawn;
        for (int y = 0; y < canvas.height(); ++y) {
            const QRgb *row = reinterpret_cast<const QRgb *>(canvas.constScanLine(y));
            for (int x = 0; x < canvas.width(); ++x) {
                if (qAlpha(row[x])) {
                    drawn |= QRect(x, y, 1, 1);
                }
            }
        }
        if (!drawn.isEmpty()) {
            glyph->coverage = canvas.copy(drawn).convertToFormat(QImage::Format_Alpha8);
            glyph->offset = area.topLeft() + drawn.topLeft();
        }
    }

    // now and then, forget what nothing shows any more
    if (glyphs.size() > 256) {
        glyphs.removeIf([](const auto &entry) { return entry.value().expired(); });
    }
    glyphs.insert(key, glyph);
    return glyph;
}

/// a page of a window's symbol atlas: four planes of coverage, one in each
/// channel of a texel, each packed in shelves
class SymbolPage {
public:
    SymbolPage(QQuickWindow *window, int side)
        : m_window(window)
        , m_side(side)
        // premultiplied, so nothing multiplies the other three planes by the
        // fourth on the way to the gpu
        , m_image(side, side, QImage::Format_RGBA8888_Premultiplied)
    {
        m_image.fill(0);
    }

    int side() const { return m_side; }

    ~SymbolPage() { delete m_texture; }

    /// room for `size` texels in one of the planes, and which
    std::optional<std::pair<QPoint, int>> allocate(QSize size)
    {
        for (int plane = 0; plane < 4; ++plane) {
            Plane &packed = m_planes[plane];
            for (Shelf &shelf : packed.shelves) {
                // a shelf takes what is no shorter than three quarters of it,
                // so small glyphs do not strand the room above them
                if (size.height() <= shelf.height && size.height() * 4 >= shelf.height * 3
                    && shelf.x + size.width() <= m_side) {
                    const QPoint at(shelf.x, shelf.y);
                    shelf.x += size.width();
                    return std::pair {at, plane};
                }
            }
            if (packed.bottom + size.height() <= m_side && size.width() <= m_side) {
                packed.shelves.append({packed.bottom, size.height(), size.width()});
                const QPoint at(0, packed.bottom);
                packed.bottom += size.height();
                return std::pair {at, plane};
            }
        }
        m_full = true;
        return std::nullopt;
    }

    void write(const QImage &coverage, QPoint at, int plane)
    {
        for (int y = 0; y < coverage.height(); ++y) {
            const uchar *from = coverage.constScanLine(y);
            uchar *to = m_image.scanLine(at.y() + y) + at.x() * 4 + plane;
            for (int x = 0; x < coverage.width(); ++x) {
                to[x * 4] = from[x];
            }
        }
        m_dirty = true;
    }

    bool isFull() const { return m_full; }

    /// the page as the gpu has it, sent again whole when a glyph was added
    /// since: that is only as icons not seen before appear
    QSGTexture *texture(QRhi *rhi, QRhiResourceUpdateBatch *updates)
    {
        if (m_dirty || !m_texture) {
            // what the gpu still draws from goes once those frames are done
            delete m_texture;
            m_texture = m_window->createTextureFromImage(m_image);
            m_texture->setFiltering(QSGTexture::Linear);
            m_texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            m_texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
            m_dirty = false;
            // a full page never changes again; the texture keeps what it sends
            if (m_full) {
                m_image = QImage();
            }
        }
        m_texture->commitTextureOperations(rhi, updates);
        return m_texture;
    }

private:
    struct Shelf {
        int y;
        int height;
        int x;
    };
    struct Plane {
        QList<Shelf> shelves;
        int bottom = 0;
    };

    QQuickWindow *m_window;
    int m_side;
    QImage m_image;
    QSGTexture *m_texture = nullptr;
    Plane m_planes[4];
    bool m_dirty = true;
    bool m_full = false;
};

/// every glyph a window's icons show, on as few pages as hold them. a window
/// draws on a thread of its own, and only that thread touches its atlas
class SymbolAtlas {
public:
    struct Slot {
        SymbolPage *page = nullptr;
        /// the glyph's texels, inside its padding
        QRect rect;
        int plane = 0;
    };

    explicit SymbolAtlas(QQuickWindow *window)
        : m_window(window)
    {
    }

    ~SymbolAtlas() { qDeleteAll(m_pages); }

    Slot place(const SymbolGlyph &glyph)
    {
        if (const auto found = m_slots.constFind(glyph.key); found != m_slots.cend()) {
            return *found;
        }
        const QSize padded = glyph.coverage.size() + QSize(2 * kPadding, 2 * kPadding);
        SymbolPage *page = nullptr;
        std::optional<std::pair<QPoint, int>> room;
        if (padded.width() > kPageSide || padded.height() > kPageSide) {
            // a page of its own, which nothing else goes on
            page = new SymbolPage(m_window, std::max(padded.width(), padded.height()));
            m_pages.append(page);
            room = page->allocate(padded);
            page->allocate(QSize(page->side() + 1, page->side() + 1));
        } else {
            page = m_open;
            if (page) {
                room = page->allocate(padded);
            }
            if (!room) {
                page = m_open = new SymbolPage(m_window, kPageSide);
                m_pages.append(page);
                room = page->allocate(padded);
            }
        }
        const QPoint at = room->first + QPoint(kPadding, kPadding);
        page->write(glyph.coverage, at, room->second);
        const Slot slot {page, QRect(at, glyph.coverage.size()), room->second};
        m_slots.insert(glyph.key, slot);
        return slot;
    }

private:
    QQuickWindow *m_window;
    QList<SymbolPage *> m_pages;
    /// the page new glyphs go on
    SymbolPage *m_open = nullptr;
    QHash<QString, Slot> m_slots;
};

QMutex atlasLock;
QHash<QQuickWindow *, SymbolAtlas *> atlases;

/// one atlas a window, let go of with the window's scene graph
SymbolAtlas *atlasFor(QQuickWindow *window)
{
    QMutexLocker locker(&atlasLock);
    if (SymbolAtlas *atlas = atlases.value(window)) {
        return atlas;
    }
    auto *atlas = new SymbolAtlas(window);
    atlases.insert(window, atlas);
    QObject::connect(window, &QQuickWindow::sceneGraphInvalidated, window, [window] {
        QMutexLocker locker(&atlasLock);
        delete atlases.take(window);
    }, Qt::DirectConnection);
    return atlas;
}

class SymbolMaterial : public QSGMaterial {
public:
    explicit SymbolMaterial(SymbolPage *page)
        : page(page)
    {
        setFlag(Blending);
    }

    QSGMaterialType *type() const override
    {
        static QSGMaterialType kind;
        return &kind;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial *other) const override
    {
        const SymbolPage *those = static_cast<const SymbolMaterial *>(other)->page;
        return page == those ? 0 : (page < those ? -1 : 1);
    }

    SymbolPage *page;
};

class SymbolShader : public QSGMaterialShader {
public:
    SymbolShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/material_symbol.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/material_symbol.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override
    {
        bool changed = false;
        char *data = state.uniformData()->data();
        if (state.isMatrixDirty()) {
            memcpy(data, state.modelViewMatrix().constData(), 64);
            memcpy(data + 64, state.projectionMatrix().constData(), 64);
            const float ratio = state.devicePixelRatio();
            memcpy(data + 132, &ratio, 4);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            memcpy(data + 128, &opacity, 4);
            changed = true;
        }
        return changed;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture, QSGMaterial *newMaterial,
                            QSGMaterial *) override
    {
        if (binding != 1) {
            return;
        }
        *texture = static_cast<SymbolMaterial *>(newMaterial)->page->texture(state.rhi(), state.resourceUpdateBatch());
    }
};

QSGMaterialShader *SymbolMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new SymbolShader;
}

struct SymbolVertex {
    float x, y;
    /// where in the atlas page
    float u, v;
    /// a 1 in the channel of the glyph's plane
    float r0, g0, b0, a0;
    /// premultiplied
    float r, g, b, a;
};

const QSGGeometry::AttributeSet &symbolAttributes()
{
    static const QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
        QSGGeometry::Attribute::createWithAttributeType(3, 4, QSGGeometry::FloatType, QSGGeometry::ColorAttribute),
    };
    static const QSGGeometry::AttributeSet set = {4, sizeof(SymbolVertex), attributes};
    return set;
}

} // namespace

MaterialSymbolItem::MaterialSymbolItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setImplicitSize(m_iconSize, m_iconSize);
    if (Settings *settings = Settings::instance()) {
        connect(settings, &Settings::zoomChanged, this, &MaterialSymbolItem::reshape);
    }
}

MaterialSymbolItem::~MaterialSymbolItem() = default;

void MaterialSymbolItem::setText(const QString &text)
{
    if (text == m_text) {
        return;
    }
    m_text = text;
    emit textChanged();
    reshape();
}

void MaterialSymbolItem::setIconSize(qreal size)
{
    if (size == m_iconSize) {
        return;
    }
    m_iconSize = size;
    setImplicitSize(size, size);
    emit iconSizeChanged();
    reshape();
}

void MaterialSymbolItem::setFill(qreal fill)
{
    if (fill == m_fill) {
        return;
    }
    const bool filled = (fill >= 0.5) != (m_fill >= 0.5);
    m_fill = fill;
    emit fillChanged();
    if (filled) {
        reshape();
    }
}

void MaterialSymbolItem::setWeight(int weight)
{
    if (weight == m_weight) {
        return;
    }
    m_weight = weight;
    emit weightChanged();
    reshape();
}

void MaterialSymbolItem::setGrade(qreal grade)
{
    if (grade == m_grade) {
        return;
    }
    m_grade = grade;
    emit gradeChanged();
    reshape();
}

void MaterialSymbolItem::setColor(const QColor &color)
{
    if (color == m_color) {
        return;
    }
    m_color = color;
    emit colorChanged();
    update();
}

void MaterialSymbolItem::reshape()
{
    m_stale = true;
    polish();
}

void MaterialSymbolItem::updatePolish()
{
    if (!m_stale) {
        return;
    }
    m_stale = false;
    const qreal zoom = Settings::instance() ? Settings::instance()->zoom() : 1.0;
    const qreal ratio = (window() ? window()->effectiveDevicePixelRatio() : 1) * (zoom > 0 ? zoom : 1.0);
    m_glyph = m_text.isEmpty() ? nullptr : glyphFor(m_text, m_fill, m_weight, m_grade, m_iconSize, ratio);
    update();
}

void MaterialSymbolItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if ((change == ItemSceneChange && data.window) || change == ItemDevicePixelRatioHasChanged) {
        if (Settings *settings = Settings::instance()) {
            disconnect(settings, &Settings::zoomChanged, this, &MaterialSymbolItem::reshape);
            connect(settings, &Settings::zoomChanged, this, &MaterialSymbolItem::reshape);
        }
        reshape();
    }
}

void MaterialSymbolItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}

QSGNode *MaterialSymbolItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    const SymbolGlyph *glyph = m_glyph.get();
    QQuickWindow *window = this->window();
    if (!glyph || glyph->coverage.isNull() || m_color.alpha() == 0 || !window) {
        delete oldNode;
        return nullptr;
    }
    const SymbolAtlas::Slot slot = atlasFor(window)->place(*glyph);
    if (!slot.page) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGGeometryNode *>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(symbolAttributes(), 4);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new SymbolMaterial(slot.page));
        node->setFlag(QSGNode::OwnsMaterial);
    } else if (auto *material = static_cast<SymbolMaterial *>(node->material()); material->page != slot.page) {
        material->page = slot.page;
        node->markDirty(QSGNode::DirtyMaterial);
    }

    // where a Text centred in the item puts the line's left end on its
    // baseline, and the glyph's coverage from there
    const qreal ratio = glyph->devicePixelRatio;
    const qreal originX = (width() - glyph->advance) / 2;
    const qreal baseline = (height() - glyph->lineHeight) / 2 + glyph->ascent;
    const float left = float(originX + glyph->offset.x() / ratio);
    const float top = float(baseline + glyph->offset.y() / ratio);
    const float right = left + float(glyph->coverage.width() / ratio);
    const float bottom = top + float(glyph->coverage.height() / ratio);
    const float side = float(slot.page->side());
    const float u0 = float(slot.rect.x()) / side;
    const float v0 = float(slot.rect.y()) / side;
    const float u1 = float(slot.rect.x() + slot.rect.width()) / side;
    const float v1 = float(slot.rect.y() + slot.rect.height()) / side;
    float plane[4] = {0, 0, 0, 0};
    plane[slot.plane] = 1;
    const float alpha = m_color.alphaF();
    const float red = m_color.redF() * alpha;
    const float green = m_color.greenF() * alpha;
    const float blue = m_color.blueF() * alpha;

    auto *vertices = static_cast<SymbolVertex *>(node->geometry()->vertexData());
    const float corners[4][4] = {{left, top, u0, v0}, {right, top, u1, v0}, {left, bottom, u0, v1}, {right, bottom, u1, v1}};
    for (int index = 0; index < 4; ++index) {
        const float *corner = corners[index];
        vertices[index] = {corner[0], corner[1], corner[2], corner[3], plane[0], plane[1], plane[2], plane[3],
                           red, green, blue, alpha};
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

} // namespace swiftify
