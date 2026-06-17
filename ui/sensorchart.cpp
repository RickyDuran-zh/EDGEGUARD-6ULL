// SensorChart implementation
// Bezier-smooth scrolling line chart for sensor data

#include "sensorchart.h"
#include <QPainter>
#include <QFontMetrics>
#include <QtMath>

SensorChart::SensorChart(QWidget *parent)
    : QWidget(parent),
      m_visiblePoints(60),
      m_startIndex(0),
      m_minValue(0), m_maxValue(100),
      m_title("Sensor Waveform"),
      m_xLabel("Time"), m_yLabel("Value"),
      m_bgColor("#101d2f"),
      m_gridColor("#263b58"),
      m_curveColor("#02a7f0"),
      m_textColor("#8fb3d9")
{
    setMinimumSize(400, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    recalcLayout();
}

void SensorChart::setRange(double min, double max)
{
    m_minValue = min;
    m_maxValue = max;
    update();
}

void SensorChart::setLabels(const QString &xLabel, const QString &yLabel)
{
    m_xLabel = xLabel;
    m_yLabel = yLabel;
    recalcLayout();
    update();
}

void SensorChart::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void SensorChart::pushData(double value)
{
    value = qBound(m_minValue, value, m_maxValue);
    m_data.append(value);

    // Auto-scroll: keep only visible range + some buffer
    int maxBuf = m_visiblePoints * 2;
    while (m_data.size() > maxBuf) {
        m_data.removeFirst();
        if (m_startIndex > 0) m_startIndex--;
    }

    // Ensure visible window shows latest data
    m_startIndex = qMax(0, m_data.size() - m_visiblePoints);

    // Rebuild curve
    QList<QPointF> points;
    int endIdx = qMin(m_startIndex + m_visiblePoints, m_data.size());
    double xStep = m_plotRect.width() / (double)(m_visiblePoints - 1);
    double yRange = m_maxValue - m_minValue;
    if (yRange <= 0) yRange = 1;

    for (int i = m_startIndex; i < endIdx; ++i) {
        double x = m_plotRect.left() + xStep * (i - m_startIndex);
        double frac = (m_data[i] - m_minValue) / yRange;
        double y = m_plotRect.bottom() - frac * m_plotRect.height();
        points.append(QPointF(x, y));
    }

    if (points.size() >= 2)
        m_curvePath = buildSmoothCurve(points);
    else
        m_curvePath = QPainterPath();

    update();
}

void SensorChart::clearData()
{
    m_data.clear();
    m_startIndex = 0;
    m_curvePath = QPainterPath();
    update();
}

void SensorChart::recalcLayout()
{
    int leftMargin  = 50;
    int rightMargin = 20;
    int topMargin   = 30;
    int bottomMargin = 30;

    m_plotRect = QRectF(leftMargin, topMargin,
                         width() - leftMargin - rightMargin,
                         height() - topMargin - bottomMargin);
}

void SensorChart::resizeEvent(QResizeEvent *)
{
    recalcLayout();
    // Rebuild curve data points with new geometry
    if (!m_data.isEmpty())
        pushData(m_data.last());  // triggers rebuild with new rect
}

QPainterPath SensorChart::buildSmoothCurve(const QList<QPointF> &points)
{
    QPainterPath path;
    int n = points.size();
    if (n < 2) return path;

    path.moveTo(points[0]);

    if (n == 2) {
        // Straight line
        path.lineTo(points[1]);
    } else {
        // Simple Catmull-Rom to Bezier (no matrix solver needed)
        for (int i = 0; i < n - 1; ++i) {
            QPointF p0 = points[qMax(0, i - 1)];
            QPointF p1 = points[i];
            QPointF p2 = points[i + 1];
            QPointF p3 = points[qMin(n - 1, i + 2)];

            // Convert Catmull-Rom segment to cubic Bezier control points
            QPointF c1(p1.x() + (p2.x() - p0.x()) / 6.0,
                       p1.y() + (p2.y() - p0.y()) / 6.0);
            QPointF c2(p2.x() - (p3.x() - p1.x()) / 6.0,
                       p2.y() - (p3.y() - p1.y()) / 6.0);

            path.cubicTo(c1, c2, p2);
        }
    }

    // Close the bottom: fill area under curve
    QPointF last = points.last();
    QPointF first = points.first();
    path.lineTo(last.x(), m_plotRect.bottom());
    path.lineTo(m_plotRect.left(), m_plotRect.bottom());
    path.lineTo(first.x(), first.y());
    path.setFillRule(Qt::WindingFill);

    return path;
}

void SensorChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    // --- Background ---
    p.fillRect(rect(), m_bgColor);

    // --- Title ---
    QFont titleFont("Source Han Serif SC", 12);
    titleFont.setWeight(QFont::Bold);
    p.setFont(titleFont);
    p.setPen(QColor("#ffffff"));
    p.drawText(QRect(10, 4, width() - 20, 22), Qt::AlignLeft | Qt::AlignVCenter, m_title);

    // --- Plot area border ---
    QPen gridPen(m_gridColor, 0.5);
    p.setPen(gridPen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(m_plotRect);

    // --- Horizontal grid lines (5 lines) ---
    p.setPen(QPen(m_gridColor, 0.5, Qt::DotLine));
    int nYLines = 5;
    QFont tickFont("Source Han Serif SC", 7);
    p.setFont(tickFont);
    for (int i = 0; i <= nYLines; ++i) {
        double y = m_plotRect.top() + m_plotRect.height() * i / nYLines;
        p.drawLine(QPointF(m_plotRect.left(), y), QPointF(m_plotRect.right(), y));

        // Y-axis label
        double val = m_maxValue - (m_maxValue - m_minValue) * i / nYLines;
        QString label = QString::number(val, 'f', (m_maxValue - m_minValue) < 1 ? 1 : 0);
        QRect labelRect(m_plotRect.left() - 48, (int)y - 8, 44, 14);
        p.setPen(m_textColor);
        p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, label);
        p.setPen(QPen(m_gridColor, 0.5, Qt::DotLine));
    }

    // --- Vertical grid lines (6 lines) ---
    int nXLines = 6;
    for (int i = 0; i <= nXLines; ++i) {
        double x = m_plotRect.left() + m_plotRect.width() * i / nXLines;
        p.drawLine(QPointF(x, m_plotRect.top()), QPointF(x, m_plotRect.bottom()));

        // X-axis time label
        int age = m_visiblePoints - (m_visiblePoints * i / nXLines);
        QString label = QString("-%1s").arg(age / 2);  // 500ms per tick
        p.setPen(m_textColor);
        QRect labelRect((int)x - 20, (int)m_plotRect.bottom() + 2, 40, 14);
        p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, label);
        p.setPen(QPen(m_gridColor, 0.5, Qt::DotLine));
    }

    // --- Axis labels ---
    QFont axisFont("Source Han Serif SC", 8);
    p.setFont(axisFont);
    p.setPen(m_textColor);

    // Y-axis label (vertical text — simplified: draw horizontally left of axis)
    // Actually just draw it in the margin

    // X-axis label
    p.drawText(QRect(m_plotRect.left(), (int)m_plotRect.bottom() + 16,
                     (int)m_plotRect.width(), 14),
               Qt::AlignHCenter, m_xLabel);

    // --- Data curve ---
    if (!m_curvePath.isEmpty()) {
        p.setPen(Qt::NoPen);
        // Gradient fill under curve
        QLinearGradient grad(0, m_plotRect.top(), 0, m_plotRect.bottom());
        grad.setColorAt(0, QColor(m_curveColor.red(), m_curveColor.green(),
                                  m_curveColor.blue(), 180));
        grad.setColorAt(1, QColor(m_curveColor.red(), m_curveColor.green(),
                                  m_curveColor.blue(), 20));
        p.setBrush(grad);
        p.drawPath(m_curvePath);

        // Curve line itself
        if (m_data.size() >= 2) {
            QPen curvePen(m_curveColor, 2.0);
            p.setPen(curvePen);
            p.setBrush(Qt::NoBrush);

            // Draw just the top curve (not the filled bottom part)
            QPainterPath linePath;
            int endIdx = qMin(m_startIndex + m_visiblePoints, m_data.size());
            double xStep = m_plotRect.width() / (double)(m_visiblePoints - 1);
            double yRange = m_maxValue - m_minValue;
            if (yRange <= 0) yRange = 1;

            QList<QPointF> pts;
            for (int i = m_startIndex; i < endIdx; ++i) {
                double x = m_plotRect.left() + xStep * (i - m_startIndex);
                double y = m_plotRect.bottom() - (m_data[i] - m_minValue) / yRange * m_plotRect.height();
                pts.append(QPointF(x, y));
            }

            if (pts.size() >= 2) {
                linePath.moveTo(pts[0]);
                if (pts.size() == 2) {
                    linePath.lineTo(pts[1]);
                } else {
                    for (int i = 0; i < pts.size() - 1; ++i) {
                        QPointF p0 = pts[qMax(0, i - 1)];
                        QPointF p1 = pts[i];
                        QPointF p2 = pts[i + 1];
                        QPointF p3 = pts[qMin(pts.size() - 1, i + 2)];
                        QPointF c1(p1.x() + (p2.x() - p0.x()) / 6.0,
                                   p1.y() + (p2.y() - p0.y()) / 6.0);
                        QPointF c2(p2.x() - (p3.x() - p1.x()) / 6.0,
                                   p2.y() - (p3.y() - p1.y()) / 6.0);
                        linePath.cubicTo(c1, c2, p2);
                    }
                }
                p.drawPath(linePath);
            }
        }

        // --- Current value dot ---
        if (!m_data.isEmpty()) {
            double lastX = m_plotRect.left() + m_plotRect.width();
            double lastFrac = (m_data.last() - m_minValue) / (m_maxValue - m_minValue);
            double lastY = m_plotRect.bottom() - lastFrac * m_plotRect.height();
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#ffffff"));
            p.drawEllipse(QPointF(lastX, lastY), 3.5, 3.5);

            // Value label near the dot
            QFont vFont("Source Han Serif SC", 8);
            vFont.setWeight(QFont::Bold);
            p.setFont(vFont);
            p.setPen(QColor("#ffffff"));
            QString vStr = QString::number(m_data.last(), 'f', 1);
            p.drawText(QRectF(lastX - 30, lastY - 22, 60, 16),
                       Qt::AlignHCenter, vStr);
        }
    } else {
        // No data: show placeholder
        p.setPen(m_textColor);
        QFont phFont("Source Han Serif SC", 11);
        p.setFont(phFont);
        p.drawText(m_plotRect, Qt::AlignCenter, "等待数据...");
    }
}
