// SensorChart — real-time sensor data waveform for EdgeGuard
// Simplified from qt_demo QtCustomPlot, with cubic Bezier smoothing
// Fits one full page (800×480), shows one curve with axis labels

#ifndef SENSORCHART_H
#define SENSORCHART_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QPainterPath>
#include <QColor>

class SensorChart : public QWidget
{
    Q_OBJECT

public:
    explicit SensorChart(QWidget *parent = nullptr);

    /// Set the data range (min/max on Y axis)
    void setRange(double min, double max);

    /// Set axis labels
    void setLabels(const QString &xLabel, const QString &yLabel);

    /// Set chart title shown at top
    void setTitle(const QString &title);

    /// Push a new data point (auto-scrolls X axis)
    void pushData(double value);

    /// Clear all data
    void clearData();

    /// Number of visible X points
    void setVisiblePoints(int n) { m_visiblePoints = n; }

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void recalcLayout();
    QPainterPath buildSmoothCurve(const QList<QPointF> &points);

    QVector<double> m_data;        // data buffer
    int    m_visiblePoints;        // how many points visible on screen
    int    m_startIndex;           // leftmost visible data index

    double m_minValue;
    double m_maxValue;

    QString m_title;
    QString m_xLabel;
    QString m_yLabel;

    QRectF m_plotRect;             // pixel rect of the chart area
    QPainterPath m_curvePath;      // cached smooth curve

    // Colors
    QColor m_bgColor;
    QColor m_gridColor;
    QColor m_curveColor;
    QColor m_textColor;
};

#endif // SENSORCHART_H
