// CircularGauge implementation
// 270° arc from bottom-left (-135°) clockwise to bottom-right (+135°)

#include "circulargauge.h"
#include <QPainter>
#include <QFontMetrics>
#include <QtMath>

CircularGauge::CircularGauge(QWidget *parent)
    : QWidget(parent),
      m_value(0), m_min(0), m_max(100), m_unit("%"), m_title("Gauge")
{
    m_anim = new QPropertyAnimation(this, "value");
    m_anim->setDuration(200);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    setMinimumSize(110, 110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CircularGauge::setRange(int min, int max)
{
    m_min = min;
    m_max = max;
    update();
}

void CircularGauge::setValue(int value)
{
    if (m_anim->state() == QPropertyAnimation::Running)
        m_anim->stop();

    m_anim->setStartValue(m_value);
    m_anim->setEndValue(value);
    m_anim->start();
}

void CircularGauge::setUnit(const QString &unit)
{
    m_unit = unit;
    update();
}

void CircularGauge::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void CircularGauge::addZone(int pctThreshold, const QColor &color)
{
    m_zones.append(qMakePair(pctThreshold, color));
    // keep sorted descending by threshold
    std::sort(m_zones.begin(), m_zones.end(),
              [](const QPair<int,QColor> &a, const QPair<int,QColor> &b) {
                  return a.first > b.first;
              });
}

void CircularGauge::SetValue(int v)
{
    m_value = v;
    update();
}

void CircularGauge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // Layout: title at top, arc in center, value in center of arc
    QFont titleFont("Source Han Serif SC", 10);
    titleFont.setWeight(QFont::Medium);

    // --- Title ---
    p.setFont(titleFont);
    p.setPen(QColor("#8fb3d9"));
    QRect titleRect(4, 4, w - 8, 18);
    p.drawText(titleRect, Qt::AlignHCenter | Qt::AlignTop, m_title);

    // --- Arc geometry ---
    int arcTop = 22;
    int arcBottom = h - 4;
    int arcHeight = arcBottom - arcTop;
    int diameter = qMin(w - 16, arcHeight);
    int cx = w / 2;
    int cy = arcTop + arcHeight / 2;
    int r = diameter / 2;

    // Adjust radius so everything fits
    r = qMin(r, (arcHeight / 2) - 2);
    if (r < 30) r = 30;

    QRectF arcRect(cx - r, cy - r, r * 2, r * 2);

    int startAngle = 135 * 16;   // -135° from top → Qt: 135*16
    int spanAngle  = 270 * 16;   // 270° total

    // --- Color zone arcs ---
    int pct = (m_max > m_min) ? (m_value - m_min) * 100 / (m_max - m_min) : 0;
    pct = qBound(0, pct, 100);

    // Determine active color from zones
    QColor activeColor("#4ade80");  // default green
    for (const auto &zone : m_zones) {
        if (pct >= zone.first) {
            activeColor = zone.second;
            break;
        }
    }

    // Draw background arc (dim grey)
    QPen bgPen(QColor("#1a3050"), 6, Qt::SolidLine, Qt::RoundCap);
    p.setPen(bgPen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(arcRect, startAngle, spanAngle);

    // Draw value arc (colored)
    int valueSpan = -spanAngle * pct / 100;  // negative = clockwise from start
    QPen valPen(activeColor, 6, Qt::SolidLine, Qt::RoundCap);
    p.setPen(valPen);
    p.drawArc(arcRect, startAngle, valueSpan);

    // --- Tick marks (optional, small dashes at 0, 25, 50, 75, 100%) ---
    p.setPen(QPen(QColor("#3a6090"), 1));
    for (int tick = 0; tick <= 100; tick += 25) {
        double angle = (135.0 - (270.0 * tick / 100.0)) * M_PI / 180.0;
        double innerR = r - 10;
        double outerR = r - 4;
        int x1 = cx + (int)(innerR * cos(angle));
        int y1 = cy - (int)(innerR * sin(angle));
        int x2 = cx + (int)(outerR * cos(angle));
        int y2 = cy - (int)(outerR * sin(angle));
        p.drawLine(x1, y1, x2, y2);
    }

    // --- Center value text ---
    QFont valFont("Source Han Serif SC", 12);
    valFont.setWeight(QFont::Bold);
    p.setFont(valFont);
    p.setPen(QColor("#ffffff"));

    QString valStr = QString::number(m_value);
    QFontMetrics valFm(valFont);
    int valW = valFm.horizontalAdvance(valStr);

    QFont unitFont("Source Han Serif SC", 7);
    unitFont.setWeight(QFont::Normal);
    p.setFont(unitFont);
    QFontMetrics unitFm(unitFont);
    int unitW = unitFm.horizontalAdvance(m_unit);

    int totalW = valW + unitW + 1;
    int textX = cx - totalW / 2;
    int textY = cy - valFm.height() / 2 - 2;

    // Value number
    p.setFont(valFont);
    p.setPen(QColor("#ffffff"));
    p.drawText(textX, textY, valW + 2, valFm.height(), Qt::AlignRight | Qt::AlignVCenter, valStr);

    // Unit
    p.setFont(unitFont);
    p.setPen(QColor("#8fb3d9"));
    p.drawText(textX + valW + 1, textY + valFm.height() - unitFm.height(),
               unitW + 2, unitFm.height(), Qt::AlignLeft | Qt::AlignVCenter, m_unit);

    // --- Min / Max labels ---
    QFont labelFont("Source Han Serif SC", 6);
    p.setFont(labelFont);
    p.setPen(QColor("#5a7a9a"));
    // Min at left
    double minAngle = 135.0 * M_PI / 180.0;
    int minX = cx + (int)((r + 10) * cos(minAngle));
    int minY = cy - (int)((r + 10) * sin(minAngle));
    p.drawText(QRect(minX - 14, minY - 8, 28, 14), Qt::AlignCenter, QString::number(m_min));
    // Max at right
    double maxAngle = (135.0 - 270.0) * M_PI / 180.0;
    int maxX = cx + (int)((r + 10) * cos(maxAngle));
    int maxY = cy - (int)((r + 10) * sin(maxAngle));
    p.drawText(QRect(maxX - 14, maxY - 8, 28, 14), Qt::AlignCenter, QString::number(m_max));
}
