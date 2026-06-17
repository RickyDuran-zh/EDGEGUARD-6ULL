// CircularGauge — 270° arc gauge for EdgeGuard dashboard
// Inspired by qt_demo CircularProgressBar, simplified for 800×480 linuxfb
// Pure QPainter, no image dependencies, supports color zones

#ifndef CIRCULARGAUGE_H
#define CIRCULARGAUGE_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QColor>
#include <QPair>
#include <QList>

class CircularGauge : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE SetValue)

public:
    explicit CircularGauge(QWidget *parent = nullptr);

    void setRange(int min, int max);
    void setValue(int value);
    void setUnit(const QString &unit);
    void setTitle(const QString &title);

    /// Add a color zone: value >= threshold paints arc in that color
    /// Example: addZone(60, "#4ade80")  → green for 60-100%
    ///          addZone(30, "#ffd166")  → yellow for 30-60%
    ///          addZone(0,  "#ff5c5c")  → red for 0-30%
    void addZone(int pctThreshold, const QColor &color);

    int value() const { return m_value; }

protected:
    void paintEvent(QPaintEvent *) override;

private slots:
    void SetValue(int v);

private:
    int m_value;
    int m_min;
    int m_max;
    QString m_title;
    QString m_unit;
    QPropertyAnimation *m_anim;
    QList<QPair<int, QColor>> m_zones;  // sorted: highest threshold first
};

#endif // CIRCULARGAUGE_H
