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

#include "render/SpectrumItem.h"

#include "app/Settings.h"
#include "render/SpectrumSource.h"
#include "theme/PlayerStyle.h"

#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGMaterialShader>
#include <QSGOpacityNode>

#include <cmath>
#include <utility>

namespace swiftify {

namespace {

constexpr float kPi = 3.14159265358979f;
/// pull on a paused bar, levels per second squared: a full bar lands in about half a second
constexpr float kPauseGravity = 8;
constexpr int kPauseFadeMs = 350;

/// a ring's cut along the round cover's squiggle line (ArtworkImage), in
/// the item's own coordinates: what neither material draws inside of
struct SpectrumCut {
    float centerX = 0;
    float centerY = 0;
    float radius = 0;
    float amplitude = 0;
    float waves = 0;
    float phase = 0;

    bool operator==(const SpectrumCut &) const = default;

    /// the two vec4s the shaders take: (centre, circle, amplitude) and
    /// (waves, phase, enabled, unused)
    void write(char *data) const
    {
        const float values[8] = {centerX, centerY, radius, amplitude, waves, phase, waves > 0.5f ? 1.0f : 0.0f, 0};
        memcpy(data, values, sizeof values);
    }
};

int compareCuts(const SpectrumCut &a, const SpectrumCut &b)
{
    return a == b ? 0 : (memcmp(&a, &b, sizeof a) < 0 ? -1 : 1);
}

// both materials keep the scene graph from merging their nodes into a batch,
// which would move the vertices out of the item's own coordinates that the
// cut is worked out in
class SpectrumBarMaterial : public QSGMaterial {
public:
    SpectrumBarMaterial() { setFlag(Blending); setFlag(RequiresFullMatrix); }

    QSGMaterialType *type() const override
    {
        static QSGMaterialType kind;
        return &kind;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial *other) const override
    {
        const auto *that = static_cast<const SpectrumBarMaterial *>(other);
        if (glowSigma != that->glowSigma) return glowSigma < that->glowSigma ? -1 : 1;
        if (glowOpacity != that->glowOpacity) return glowOpacity < that->glowOpacity ? -1 : 1;
        if (pixelsPerPoint != that->pixelsPerPoint) return pixelsPerPoint < that->pixelsPerPoint ? -1 : 1;
        return compareCuts(cut, that->cut);
    }

    float glowSigma = 0;
    float glowOpacity = 0;
    float pixelsPerPoint = 1;
    SpectrumCut cut;
};

class SpectrumBarShader : public QSGMaterialShader {
public:
    SpectrumBarShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/spectrum_bars.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/spectrum_bars.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *) override
    {
        QByteArray *buffer = state.uniformData();
        char *data = buffer->data();
        if (state.isMatrixDirty()) {
            const QMatrix4x4 matrix = state.combinedMatrix();
            memcpy(data, matrix.constData(), 64);
        }
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            memcpy(data + 64, &opacity, 4);
        }
        const auto *material = static_cast<SpectrumBarMaterial *>(newMaterial);
        memcpy(data + 68, &material->pixelsPerPoint, 4);
        memcpy(data + 72, &material->glowSigma, 4);
        memcpy(data + 76, &material->glowOpacity, 4);
        material->cut.write(data + 80);
        return true;
    }
};

QSGMaterialShader *SpectrumBarMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new SpectrumBarShader;
}

/// the halo or wave: qt's vertex colour material, with the cut
class SpectrumHaloMaterial : public QSGMaterial {
public:
    SpectrumHaloMaterial() { setFlag(Blending); setFlag(RequiresFullMatrix); }

    QSGMaterialType *type() const override
    {
        static QSGMaterialType kind;
        return &kind;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial *other) const override
    {
        return compareCuts(cut, static_cast<const SpectrumHaloMaterial *>(other)->cut);
    }

    SpectrumCut cut;
};

class SpectrumHaloShader : public QSGMaterialShader {
public:
    SpectrumHaloShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/spectrum_halo.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/spectrum_halo.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *) override
    {
        char *data = state.uniformData()->data();
        if (state.isMatrixDirty()) {
            const QMatrix4x4 matrix = state.combinedMatrix();
            memcpy(data, matrix.constData(), 64);
        }
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            memcpy(data + 64, &opacity, 4);
        }
        static_cast<SpectrumHaloMaterial *>(newMaterial)->cut.write(data + 80);
        return true;
    }
};

QSGMaterialShader *SpectrumHaloMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new SpectrumHaloShader;
}

const QSGGeometry::AttributeSet &barAttributes()
{
    static const QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::FloatType, QSGGeometry::ColorAttribute),
        QSGGeometry::Attribute::createWithAttributeType(3, 4, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
    };
    static const QSGGeometry::AttributeSet set = {4, 12 * sizeof(float), attributes};
    return set;
}

/// catmull-rom through the points as cubic béziers, eight steps a segment.
/// `closed` wraps the ends; otherwise they are clamped.
QList<QPointF> smoothCurve(const QList<QPointF> &points, bool closed)
{
    QList<QPointF> curve;
    const qsizetype count = points.size();
    if (count < 2) {
        return points;
    }
    const auto at = [&](qsizetype index) {
        if (closed) {
            return points[(index % count + count) % count];
        }
        return points[std::clamp<qsizetype>(index, 0, count - 1)];
    };
    const qsizetype segments = closed ? count : count - 1;
    constexpr int steps = 8;
    for (qsizetype segment = 0; segment < segments; ++segment) {
        const QPointF p0 = at(segment - 1), p1 = at(segment), p2 = at(segment + 1), p3 = at(segment + 2);
        const QPointF c1 = p1 + (p2 - p0) / 6;
        const QPointF c2 = p2 - (p3 - p1) / 6;
        for (int step = 0; step < steps; ++step) {
            const double t = double(step) / steps;
            const double u = 1 - t;
            curve.append(u * u * u * p1 + 3 * u * u * t * c1 + 3 * u * t * t * c2 + t * t * t * p2);
        }
    }
    if (!closed) {
        curve.append(points.last());
    }
    return curve;
}

} // namespace

SpectrumItem::SpectrumItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    m_opacityAnimation.setDuration(kPauseFadeMs);
    m_opacityAnimation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_opacityAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_contentOpacity = value.toReal();
        update();
    });

    if (SpectrumSource *source = SpectrumSource::instance()) {
        connect(source, &SpectrumSource::levelsChanged, this, &SpectrumItem::retarget);
        connect(source, &SpectrumSource::loadingChanged, this, [this] {
            retarget();
            updateOpacity(true);
        });
    }
    if (PlayerStyle *style = PlayerStyle::instance()) {
        connect(style, &PlayerStyle::paletteChanged, this, [this] {
            applyPalette();
            m_geometryDirty = true;
            update();
        });
    }
    rebuildStyle();
}

SpectrumItem::~SpectrumItem()
{
    stopFrames();
}

void SpectrumItem::setKind(Kind kind)
{
    if (m_kind != kind) {
        m_kind = kind;
        rebuildStyle();
        emit styleChanged();
    }
}

void SpectrumItem::setWave(bool wave)
{
    if (m_wave != wave) {
        m_wave = wave;
        rebuildStyle();
        emit styleChanged();
    }
}

void SpectrumItem::setContinuous(bool continuous)
{
    if (m_continuous != continuous) {
        m_continuous = continuous;
        rebuildStyle();
        emit styleChanged();
    }
}

void SpectrumItem::setSensitivity(qreal sensitivity)
{
    if (m_sensitivity != sensitivity) {
        m_sensitivity = sensitivity;
        rebuildStyle();
        emit styleChanged();
    }
}

void SpectrumItem::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    // one that appears already paused starts dim; paused on screen, it dims as
    // its bars fall.
    updateOpacity(m_configured);
    retarget();
    emit activeChanged();
}

void SpectrumItem::setCut(qreal &field, qreal value)
{
    if (qFuzzyCompare(field + 1, value + 1)) {
        return;
    }
    field = value;
    emit cutChanged();
    // only the material changes; the bars stay as they are
    update();
}

void SpectrumItem::setSuspended(bool suspended)
{
    if (m_suspended == suspended) {
        return;
    }
    m_suspended = suspended;
    if (!suspended) {
        retarget();
        ensureFrames();
    }
    emit suspendedChanged();
}

void SpectrumItem::rebuildStyle()
{
    switch (m_kind) {
    case Kind::Compact: m_style = SpectrumStyle::compact(float(height() > 0 ? height() : 34)); break;
    case Kind::Immersive: m_style = m_continuous ? SpectrumStyle::continuous() : SpectrumStyle::immersive(); break;
    case Kind::Ring: m_style = SpectrumStyle::ring(); break;
    case Kind::Equalizer: m_style = SpectrumStyle::equalizer(); break;
    }
    if (m_wave && m_kind != Kind::Compact && m_kind != Kind::Equalizer) {
        m_style = m_style.asWave();
    }
    // the gain on the analyser's output, a setting rather than a rebuild
    m_style.levelScale *= float(m_sensitivity);
    applyPalette();
    retarget();
    updateOpacity(false);
    m_configured = true;
    m_geometryDirty = true;
    update();
}

bool SpectrumItem::wantsHalo() const
{
    return m_style.haloOpacity > 0 && (m_style.radial || m_style.alignment == SpectrumStyle::Alignment::Bottom);
}

void SpectrumItem::applyPalette()
{
    using namespace spectrum;
    const int count = m_style.barCount();
    QList<QColor> colors = m_style.paletteOverride;
    if (colors.isEmpty()) {
        if (PlayerStyle *style = PlayerStyle::instance()) {
            colors = style->palette();
        }
    }
    if (colors.isEmpty()) {
        colors = {QColor(0xff, 0x3d, 0x8a), QColor(0xd6, 0x24, 0x6c)};
    }
    const Rgba first = rgba(colors.first());
    const Rgba second = rgba(colors.size() > 1 ? colors.at(1) : colors.first());

    QList<Rgba> palette;
    palette.reserve(count);
    for (int index = 0; index < count; ++index) {
        const float linear = count <= 1 ? 0.0f : float(index) / float(count - 1);
        const float progress = m_style.symmetricGradient ? 1 - std::abs(linear * 2 - 1) : linear;
        Rgba mixed;
        for (int channel = 0; channel < 4; ++channel) {
            mixed[channel] = first[channel] + (second[channel] - first[channel]) * progress;
        }
        palette.append(mixed);
    }
    m_barColors = palette;
    m_haloColors = !wantsHalo() ? QList<Rgba> {} : palette;
    m_ringColorsDirty = true;
}

void SpectrumItem::retarget()
{
    // a hidden meter works nothing out: there was one in every song row,
    // each reducing the whole spectrum thirty times a second. it catches up
    // as it is shown again (itemChange)
    if (!isVisible() && !m_levels.isEmpty()) {
        return;
    }
    SpectrumSource *source = SpectrumSource::instance();
    const int count = m_style.barCount();
    m_loading = source && source->isLoading();

    QList<float> next;
    if (m_loading) {
        // bars sweep across and wait a moment; a ring pulses down both sides
        // at once, then waits longer, so it reads as a breath round the cover
        next = m_style.radial ? spectrum::ringLoadingPulse(count, source->loadingPhase(), 0.45f)
                              : spectrum::loadingPulse(count, source->loadingPhase(), 0.22f);
    } else if (!m_active || !source) {
        // paused: aim every bar at rest, and let them fall there.
        next = QList<float>(count, 0.0f);
    } else {
        switch (m_style.layout) {
        case SpectrumStyle::Layout::Compact:
            next = spectrum::compact(source->levels(), count);
            break;
        case SpectrumStyle::Layout::Mirrored: {
            const QList<float> side = spectrum::bassExpanded(source->levels(), m_style.count);
            next = side;
            for (qsizetype index = side.size() - 1; index >= 0; --index) {
                next.append(side[index]);
            }
            break;
        }
        case SpectrumStyle::Layout::Ascending:
            next = spectrum::bassExpanded(source->levels(), count);
            break;
        }
    }

    QList<float> mapped(next.size());
    float largestChange = 0;
    for (qsizetype index = 0; index < next.size(); ++index) {
        const float level = std::isfinite(next[index]) ? std::clamp(next[index], 0.0f, 1.0f) : 0.0f;
        mapped[index] = std::clamp(m_style.baseLevel + m_style.levelScale * std::pow(level, m_style.responseExponent), 0.0f, 1.0f);
        if (index < m_targets.size()) {
            largestChange = std::max(largestChange, std::abs(m_targets[index] - mapped[index]));
        }
    }
    if (m_targets.size() == mapped.size() && largestChange < 0.002f) {
        // the same bars, but it may have just started playing or loading
        ensureFrames();
        return;
    }
    m_targets = mapped;
    if (m_levels.size() != mapped.size()) {
        m_levels = mapped;
        m_fallVelocities = QList<float>(mapped.size(), 0.0f);
        m_geometryDirty = true;
        update();
    }
    ensureFrames();
}

void SpectrumItem::updateOpacity(bool animated)
{
    const qreal target = m_active || m_loading ? 1.0 : m_style.inactiveOpacity;
    if (std::abs(target - m_contentOpacity) < 0.001 && m_opacityAnimation.state() != QAbstractAnimation::Running) {
        return;
    }
    m_opacityAnimation.stop();
    if (animated) {
        m_opacityAnimation.setStartValue(m_contentOpacity);
        m_opacityAnimation.setEndValue(target);
        m_opacityAnimation.start();
    } else {
        m_contentOpacity = target;
        update();
    }
}

bool SpectrumItem::isMoving() const
{
    if (width() <= 0 || height() <= 0 || m_levels.size() != m_targets.size()) {
        return false;
    }
    for (qsizetype index = 0; index < m_levels.size(); ++index) {
        if (std::abs(m_levels[index] - m_targets[index]) >= 0.0005f) {
            return true;
        }
    }
    return false;
}

bool SpectrumItem::wantsFrames() const
{
    if (width() <= 0 || height() <= 0 || m_levels.size() != m_targets.size() || m_levels.isEmpty()) {
        return false;
    }
    // every frame the display shows while music plays or the loading pulse
    // runs, not only those an analyser update lands near; after that, only
    // until the bars have settled
    return m_active || m_loading || isMoving();
}

void SpectrumItem::ensureFrames()
{
    // and only while shown and uncovered
    if (m_suspended || !isVisible() || !window() || !wantsFrames()) {
        return;
    }
    if (!m_frameConnection) {
        m_frameClockRunning = false;
        m_frameConnection = connect(window(), &QQuickWindow::afterAnimating, this, &SpectrumItem::onFrame);
    }
    window()->update();
}

void SpectrumItem::stopFrames()
{
    disconnect(m_frameConnection);
    m_frameConnection = {};
    m_frameClockRunning = false;
}

void SpectrumItem::onFrame()
{
    if (m_suspended || !isVisible() || !wantsFrames()) {
        stopFrames();
        return;
    }
    const double elapsed = m_frameClockRunning ? m_frameClock.nsecsElapsed() / 1e9 : 1.0 / 60.0;
    m_frameClock.restart();
    m_frameClockRunning = true;
    advance(elapsed);
    update();
}

void SpectrumItem::advance(double seconds)
{
    const float frame = float(std::clamp(seconds, 1.0 / 240.0, 1.0 / 15.0));
    const bool falling = !m_active && !m_loading;
    if (m_fallVelocities.size() != m_levels.size()) {
        m_fallVelocities = QList<float>(m_levels.size(), 0.0f);
    }
    for (qsizetype index = 0; index < m_levels.size() && index < m_targets.size(); ++index) {
        const float difference = m_targets[index] - m_levels[index];
        float next;
        if (falling && difference < 0) {
            // nothing drives the bars once playback stops, so they drop under
            // gravity instead of easing out on the music's release.
            m_fallVelocities[index] += kPauseGravity * frame;
            next = std::max(m_targets[index], m_levels[index] - m_fallVelocities[index] * frame);
        } else {
            m_fallVelocities[index] = 0;
            const float constant = difference > 0 ? std::max(m_style.riseDuration, 0.005f) : std::max(m_style.animationDuration, 0.005f);
            next = m_levels[index] + difference * (1 - std::exp(-frame / constant));
        }
        if (std::abs(m_targets[index] - next) < 0.0005f) {
            m_levels[index] = m_targets[index];
            m_fallVelocities[index] = 0;
        } else {
            m_levels[index] = next;
        }
    }
    m_geometryDirty = true;
}

void SpectrumItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        if (m_kind == Kind::Compact) {
            rebuildStyle();
        }
        m_geometryDirty = true;
        update();
    }
}

void SpectrumItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemSceneChange) {
        stopFrames();
    }
    if (change == ItemVisibleHasChanged && value.boolValue) {
        retarget();
        ensureFrames();
    }
    QQuickItem::itemChange(change, value);
}

// --- geometry ------------------------------------------------------------------------------

float SpectrumItem::rangeWeight(int index, int count) const
{
    if (m_style.bassRangeFalloff <= 0 || count <= 1) {
        return 1;
    }
    float distance;
    if (m_style.layout == SpectrumStyle::Layout::Mirrored) {
        distance = float(std::min(index, count - 1 - index)) / float(std::max(count / 2 - 1, 1));
    } else {
        distance = float(index) / float(count - 1);
    }
    return 1 - m_style.bassRangeFalloff * std::min(distance, 1.0f);
}

float SpectrumItem::linearBarHeight(int index, float barWidth) const
{
    // a bar at rest reads as a dot rather than a flattened pill
    const float resting = std::max(m_style.minimumHeight, barWidth);
    const float height = float(this->height());
    return std::min(height, std::max(resting, height * m_levels[index] * rangeWeight(index, int(m_levels.size()))));
}

void SpectrumItem::appendBar(float footX, float footY, float directionX, float directionY, float width, float length,
                             float tipRadius, float footRadius, const spectrum::Rgba &color, float margin)
{
    const quint16 base = quint16(m_barVertices.size());
    const float sideX = directionY;
    const float sideY = -directionX;
    for (int corner = 0; corner < 4; ++corner) {
        const float across = (corner & 1) ? 1.0f : -1.0f;
        const float along = (corner & 2) ? 1.0f : 0.0f;
        const float localX = across * (width * 0.5f + margin);
        const float localY = -margin + (length + 2 * margin) * along;
        m_barVertices.append({footX + sideX * localX + directionX * localY, footY + sideY * localX + directionY * localY,
                              localX, localY, color[0], color[1], color[2], color[3], width, length, tipRadius, footRadius});
    }
    if (!m_keepIndices) {
        m_barIndices << base << quint16(base + 1) << quint16(base + 2) << quint16(base + 1) << quint16(base + 3) << quint16(base + 2);
    }
}

void SpectrumItem::buildGeometry()
{
    m_barVertices.clear();
    m_haloVertices.clear();
    // the triangles stay the same from frame to frame for as long as the
    // shape does: only their points move
    const bool drawn = !m_levels.isEmpty() && width() > 0 && height() > 0;
    const quint64 layout = drawn ? (quint64(m_levels.size()) << 3) | (m_style.radial ? 1 : 0) | (wantsHalo() ? 2 : 0)
                                       | (m_style.drawsBars ? 4 : 0)
                                 : 0;
    m_keepIndices = layout != 0 && layout == m_indexLayout;
    m_indexLayout = layout;
    if (!m_keepIndices) {
        m_barIndices.clear();
        m_haloIndices.clear();
    }
    if (!drawn) {
        return;
    }
    if (m_barColors.size() != m_levels.size()) {
        applyPalette();
    }
    if (m_style.radial) {
        buildRadial();
    } else {
        buildLinear();
    }
}

void SpectrumItem::buildLinear()
{
    const int count = int(m_levels.size());
    const float itemWidth = float(width());
    const float itemHeight = float(height());
    const float gapCount = float(std::max(count - 1, 1));
    const float spacing = std::min(m_style.spacing, std::max(0.0f, (itemWidth - float(count)) / gapCount));
    const float barWidth = std::max(0.0f, itemWidth - spacing * float(count - 1)) / float(count);
    const float margin = m_style.glowRadius * 3 + 1;

    if (wantsHalo()) {
        appendRowHalo(barWidth, spacing);
    }
    if (!m_style.drawsBars) {
        return;
    }
    for (int index = 0; index < count; ++index) {
        const float barHeight = linearBarHeight(index, barWidth);
        const float x = float(index) * (barWidth + spacing) + barWidth / 2;
        const bool centred = m_style.alignment == SpectrumStyle::Alignment::Center;
        const float footY = centred ? (itemHeight - barHeight) / 2 : itemHeight;
        const float directionY = centred ? 1.0f : -1.0f;
        const float radius = std::min(barWidth, barHeight) / 2;
        appendBar(x, footY, 0, directionY, barWidth, barHeight, radius, radius, m_barColors.value(index), margin);
    }
}

void SpectrumItem::buildRadial()
{
    const int count = int(m_levels.size());
    const float centerX = float(width()) / 2;
    const float centerY = float(height()) / 2;
    const float edge = float(std::min(width(), height()));
    const float innerRadius = edge / 2 * m_style.innerRadiusRatio;
    const float barWidth = std::max(1.5f, 2 * kPi * innerRadius / float(count) - m_style.spacing);
    // where a bar shows from at rest, clear of the artwork's circle
    const float restCircle = innerRadius + edge / 2 * m_style.gapRatio;
    // the round cover's edge is a squiggle cut inside its circle, with a gap
    // before the line (ArtworkImage). the feet stand under the cover, so
    // its wavy cut is where the bars appear, and they rise through the gap
    // and past the line; the cover hides the rest
    const float footCircle = innerRadius * 0.8f;
    const float hidden = restCircle - footCircle;
    const float maximumLength = std::max(1.0f, edge / 2 - restCircle);
    const float resting = std::max(m_style.minimumHeight, std::max(barWidth, maximumLength * 0.18f));
    const float travel = std::max(0.0f, maximumLength - resting);
    const float footRounding = std::min(2.0f, barWidth * 0.3f);
    const float margin = m_style.glowRadius * 3 + 1;
    const float haloSpread = edge / 2 * 0.05f;

    m_barRadii.resize(0);
    for (int index = 0; index < count; ++index) {
        const float length = hidden + resting + travel * m_levels[index] * rangeWeight(index, count);
        // clockwise from the top, in the item's y-down space, each bar half a
        // step on: a mirrored ring's last bar is its first's twin, so the
        // top sits between them and the two sides match about the upright
        const float theta = -kPi / 2 + 2 * kPi * (float(index) + 0.5f) / float(count);
        const float outwardX = std::cos(theta);
        const float outwardY = std::sin(theta);
        if (wantsHalo()) {
            m_barRadii.append(footCircle + length + haloSpread);
        }
        if (m_style.drawsBars) {
            appendBar(centerX + outwardX * footCircle, centerY + outwardY * footCircle, outwardX, outwardY, barWidth,
                      length, barWidth / 2, footRounding, m_barColors.value(index), margin);
        }
    }
    if (wantsHalo()) {
        appendRingHalo(m_barRadii, centerX, centerY, edge);
    }
}

void SpectrumItem::appendFeatheredStrip(const QList<QPointF> &curve, const QList<QPointF> &inward,
                                        const QList<spectrum::Rgba> &colors, float sigma, float opacity)
{
    // a blurred edge, built rather than blurred: rings from two sigma inside
    // the outline (solid) to two sigma outside (clear), with the coverage a
    // gaussian blur gives a straight edge at each.
    static constexpr float offsets[] = {-2, -1, 0, 1, 2};
    static constexpr float coverage[] = {0.977f, 0.841f, 0.5f, 0.159f, 0.023f};
    constexpr int rings = 5;
    const quint16 base = quint16(m_haloVertices.size());
    for (qsizetype point = 0; point < curve.size(); ++point) {
        const spectrum::Rgba &color = colors[point];
        for (int ring = 0; ring < rings; ++ring) {
            const QPointF position = curve[point] - inward[point] * (offsets[ring] * sigma);
            const float alpha = std::clamp(color[3] * coverage[ring] * opacity, 0.0f, 1.0f);
            m_haloVertices.append({float(position.x()), float(position.y()), quint8(std::lround(color[0] * alpha * 255)),
                                   quint8(std::lround(color[1] * alpha * 255)), quint8(std::lround(color[2] * alpha * 255)),
                                   quint8(std::lround(alpha * 255))});
        }
    }
    for (qsizetype point = 0; point + 1 < curve.size() && !m_keepIndices; ++point) {
        for (int ring = 0; ring + 1 < rings; ++ring) {
            const quint16 a = quint16(base + point * rings + ring);
            const quint16 b = quint16(a + rings);
            m_haloIndices << a << b << quint16(a + 1) << quint16(a + 1) << b << quint16(b + 1);
        }
    }
}

void SpectrumItem::appendRowHalo(float barWidth, float spacing)
{
    const int count = int(m_levels.size());
    const float itemWidth = float(width());
    const float itemHeight = float(height());
    const float spread = itemHeight * 0.04f;
    const float sigma = std::max(0.75f, itemHeight * m_style.haloBlur);

    // a smooth line through the bar tops, levelled out to the row's ends
    QList<QPointF> tips;
    for (int index = 0; index < count; ++index) {
        const float tipHeight = std::min(itemHeight, linearBarHeight(index, barWidth) + spread);
        tips.append(QPointF(float(index) * (barWidth + spacing) + barWidth / 2, itemHeight - tipHeight));
    }
    tips.prepend(QPointF(0, tips.first().y()));
    tips.append(QPointF(itemWidth, tips.last().y()));
    const QList<QPointF> curve = smoothCurve(tips, false);

    const float firstColorX = barWidth / 2;
    const float pitch = barWidth + spacing;
    QList<spectrum::Rgba> colors;
    QList<QPointF> inward;
    for (const QPointF &point : curve) {
        const float position = std::clamp(float((point.x() - firstColorX) / pitch), 0.0f, float(count - 1));
        const int lower = int(std::floor(position));
        const int upper = std::min(lower + 1, count - 1);
        const float fraction = position - float(lower);
        spectrum::Rgba color;
        for (int channel = 0; channel < 4; ++channel) {
            color[channel] = m_haloColors.value(lower)[channel] + (m_haloColors.value(upper)[channel] - m_haloColors.value(lower)[channel]) * fraction;
        }
        colors.append(color);
        inward.append(QPointF(0, 1));
    }

    // the solid body: from below the baseline, where the blur would fade out
    // of sight, up to where the edge begins.
    const float depth = spread + sigma * 3;
    const quint16 base = quint16(m_haloVertices.size());
    for (qsizetype point = 0; point < curve.size(); ++point) {
        const spectrum::Rgba &color = colors[point];
        const float alpha = std::clamp(color[3] * m_style.haloOpacity, 0.0f, 1.0f);
        const quint8 r = quint8(std::lround(color[0] * alpha * 255)), g = quint8(std::lround(color[1] * alpha * 255)),
                     b = quint8(std::lround(color[2] * alpha * 255)), a = quint8(std::lround(alpha * 255));
        m_haloVertices.append({float(curve[point].x()), float(curve[point].y() + 2 * sigma), r, g, b, a});
        m_haloVertices.append({float(curve[point].x()), itemHeight + depth, r, g, b, a});
    }
    for (qsizetype point = 0; point + 1 < curve.size() && !m_keepIndices; ++point) {
        const quint16 top = quint16(base + point * 2);
        m_haloIndices << top << quint16(top + 1) << quint16(top + 2) << quint16(top + 2) << quint16(top + 1) << quint16(top + 3);
    }
    appendFeatheredStrip(curve, inward, colors, sigma, m_style.haloOpacity);
}

void SpectrumItem::prepareRing(qsizetype count)
{
    constexpr int steps = 8;
    static constexpr float coverage[] = {0.977f, 0.841f, 0.5f, 0.159f, 0.023f};
    const qsizetype points = count * steps;
    if (m_ringBars != count) {
        m_ringPoints.resize(points + 1);
        for (qsizetype point = 0; point <= points; ++point) {
            // clockwise from the top, in the item's y-down space, a bar's
            // point where its bar stands (half a step on, buildRadial); the
            // last point is the first again, closing the loop
            const double theta = -kPi / 2 + 2 * kPi * (double(point % points) + steps / 2.0) / double(points);
            m_ringPoints[point].cos = float(std::cos(theta));
            m_ringPoints[point].sin = float(std::sin(theta));
        }
        m_ringBars = count;
        m_ringColorsDirty = true;
    }
    if (!m_ringColorsDirty) {
        return;
    }
    m_ringColorsDirty = false;
    const qsizetype colorCount = m_haloColors.size();
    const auto pack = [](const spectrum::Rgba &color, float alpha) {
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        return std::array<quint8, 4> {quint8(color[0] * alpha * 255 + 0.5f), quint8(color[1] * alpha * 255 + 0.5f),
                                      quint8(color[2] * alpha * 255 + 0.5f), quint8(alpha * 255 + 0.5f)};
    };
    for (qsizetype point = 0; point <= points; ++point) {
        // coloured by angle, round the palette clockwise from the top
        spectrum::Rgba color {1, 1, 1, 1};
        if (colorCount > 0) {
            const double position = double(point % points) / double(points) * double(colorCount);
            const qsizetype lower = qsizetype(std::floor(position)) % colorCount;
            const qsizetype upper = (lower + 1) % colorCount;
            const float fraction = float(position - std::floor(position));
            for (int channel = 0; channel < 4; ++channel) {
                color[channel] = m_haloColors[lower][channel] + (m_haloColors[upper][channel] - m_haloColors[lower][channel]) * fraction;
            }
        }
        RingPoint &ring = m_ringPoints[point];
        ring.colors[0] = pack(color, color[3] * m_style.haloOpacity);
        for (int edge = 0; edge < 5; ++edge) {
            ring.colors[edge + 1] = pack(color, color[3] * coverage[edge] * m_style.haloOpacity);
        }
    }
}

void SpectrumItem::appendRingHalo(const QList<float> &radii, float centerX, float centerY, float edge)
{
    const qsizetype count = radii.size();
    if (count < 3) {
        return;
    }
    constexpr int steps = 8;
    static constexpr float offsets[] = {-2, -1, 0, 1, 2};
    constexpr int rings = 5;
    prepareRing(count);

    // two passes of a 1-2-1 average round off a lone bar without flattening the ring
    m_ringRadii.resize(count);
    std::copy(radii.begin(), radii.end(), m_ringRadii.begin());
    m_ringScratch.resize(count);
    for (int pass = 0; pass < 2; ++pass) {
        for (qsizetype index = 0; index < count; ++index) {
            m_ringScratch[index] = (m_ringRadii[(index + count - 1) % count] + 2 * m_ringRadii[index]
                                    + m_ringRadii[(index + 1) % count]) / 4;
        }
        std::swap(m_ringRadii, m_ringScratch);
    }
    // a smooth outline through the bar tips: catmull-rom on the radius, round
    // the circle, eight steps between bars
    const qsizetype points = count * steps;
    m_ringCurve.resize(points + 1);
    for (qsizetype point = 0; point <= points; ++point) {
        const qsizetype segment = (point / steps) % count;
        const float t = float(point % steps) / steps;
        const float r0 = m_ringRadii[(segment + count - 1) % count];
        const float r1 = m_ringRadii[segment];
        const float r2 = m_ringRadii[(segment + 1) % count];
        const float r3 = m_ringRadii[(segment + 2) % count];
        m_ringCurve[point] = 0.5f * (2 * r1 + (r2 - r0) * t + (2 * r0 - 5 * r1 + 4 * r2 - r3) * t * t
                                     + (3 * r1 - r0 - 3 * r2 + r3) * t * t * t);
    }

    const float sigma = std::max(0.75f, edge / 2 * m_style.haloBlur);
    const auto vertex = [&](qsizetype point, float radius, int color) {
        const RingPoint &ring = m_ringPoints[point];
        const auto &rgba = ring.colors[color];
        m_haloVertices.append({centerX + ring.cos * radius, centerY + ring.sin * radius, rgba[0], rgba[1], rgba[2], rgba[3]});
    };

    // the solid middle, fanned from the centre to where the edge begins
    const quint16 centre = quint16(m_haloVertices.size());
    const auto &middle = m_ringPoints[0].colors[0];
    m_haloVertices.append({centerX, centerY, middle[0], middle[1], middle[2], middle[3]});
    for (qsizetype point = 0; point <= points; ++point) {
        vertex(point, m_ringCurve[point] - 2 * sigma, 0);
    }
    for (qsizetype point = 0; point < points && !m_keepIndices; ++point) {
        m_haloIndices << centre << quint16(centre + 1 + point) << quint16(centre + 2 + point);
    }

    // the blurred edge, built rather than blurred: rings from two sigma
    // inside the outline (solid) to two sigma outside (clear)
    const quint16 base = quint16(m_haloVertices.size());
    for (qsizetype point = 0; point <= points; ++point) {
        for (int ring = 0; ring < rings; ++ring) {
            vertex(point, m_ringCurve[point] + offsets[ring] * sigma, ring + 1);
        }
    }
    for (qsizetype point = 0; point < points && !m_keepIndices; ++point) {
        for (int ring = 0; ring + 1 < rings; ++ring) {
            const quint16 a = quint16(base + point * rings + ring);
            const quint16 b = quint16(a + rings);
            m_haloIndices << a << b << quint16(a + 1) << quint16(a + 1) << b << quint16(b + 1);
        }
    }
}

QSGNode *SpectrumItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *root = static_cast<QSGOpacityNode *>(oldNode);
    QSGGeometryNode *halo = nullptr;
    QSGGeometryNode *bars = nullptr;
    if (!root) {
        root = new QSGOpacityNode;
        halo = new QSGGeometryNode;
        auto *haloGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0, 0, QSGGeometry::UnsignedShortType);
        haloGeometry->setDrawingMode(QSGGeometry::DrawTriangles);
        halo->setGeometry(haloGeometry);
        halo->setFlag(QSGNode::OwnsGeometry);
        halo->setMaterial(new SpectrumHaloMaterial);
        halo->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(halo);

        bars = new QSGGeometryNode;
        auto *barGeometry = new QSGGeometry(barAttributes(), 0, 0, QSGGeometry::UnsignedShortType);
        barGeometry->setDrawingMode(QSGGeometry::DrawTriangles);
        bars->setGeometry(barGeometry);
        bars->setFlag(QSGNode::OwnsGeometry);
        bars->setMaterial(new SpectrumBarMaterial);
        bars->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(bars);
        m_geometryDirty = true;
    } else {
        halo = static_cast<QSGGeometryNode *>(root->firstChild());
        bars = static_cast<QSGGeometryNode *>(root->lastChild());
    }

    root->setOpacity(m_contentOpacity);

    auto *material = static_cast<SpectrumBarMaterial *>(bars->material());
    auto *haloMaterial = static_cast<SpectrumHaloMaterial *>(halo->material());
    const float zoom = Settings::instance() ? float(Settings::instance()->zoom()) : 1.0f;
    const float pixelsPerPoint = (window() ? float(window()->effectiveDevicePixelRatio()) : 1.0f) * (zoom > 0 ? zoom : 1.0f);
    SpectrumCut cut;
    if (m_style.radial && m_cutWaves > 0.5) {
        cut = {float(width()) / 2, float(height()) / 2, float(m_cutRadius), float(m_cutAmplitude), float(m_cutWaves),
               float(m_cutPhase)};
    }
    if (material->glowSigma != m_style.glowRadius || material->glowOpacity != m_style.glowOpacity
        || material->pixelsPerPoint != pixelsPerPoint || !(material->cut == cut)) {
        material->glowSigma = m_style.glowRadius;
        material->glowOpacity = m_style.glowOpacity;
        material->pixelsPerPoint = pixelsPerPoint;
        material->cut = cut;
        bars->markDirty(QSGNode::DirtyMaterial);
    }
    if (!(haloMaterial->cut == cut)) {
        haloMaterial->cut = cut;
        halo->markDirty(QSGNode::DirtyMaterial);
    }

    if (m_geometryDirty) {
        m_geometryDirty = false;
        buildGeometry();

        QSGGeometry *barGeometry = bars->geometry();
        barGeometry->allocate(int(m_barVertices.size()), int(m_barIndices.size()));
        if (!m_barVertices.isEmpty()) {
            memcpy(barGeometry->vertexData(), m_barVertices.constData(), m_barVertices.size() * sizeof(BarVertex));
            memcpy(barGeometry->indexDataAsUShort(), m_barIndices.constData(), m_barIndices.size() * sizeof(quint16));
        }
        bars->markDirty(QSGNode::DirtyGeometry);

        QSGGeometry *haloGeometry = halo->geometry();
        haloGeometry->allocate(int(m_haloVertices.size()), int(m_haloIndices.size()));
        if (!m_haloVertices.isEmpty()) {
            memcpy(haloGeometry->vertexData(), m_haloVertices.constData(), m_haloVertices.size() * sizeof(HaloVertex));
            memcpy(haloGeometry->indexDataAsUShort(), m_haloIndices.constData(), m_haloIndices.size() * sizeof(quint16));
        }
        halo->markDirty(QSGNode::DirtyGeometry);
    }
    return root;
}

} // namespace swiftify
