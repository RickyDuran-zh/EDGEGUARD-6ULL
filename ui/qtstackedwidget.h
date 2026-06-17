// QtStackedWidget — swipeable stacked container with QPropertyAnimation
// Simplified from qt_demo QtUi/src/qtstackedwidget.h
// Horizontal (left/right) swipe only, 10% threshold snap-back, edge detection

#ifndef QTSTACKEDWIDGET_H
#define QTSTACKEDWIDGET_H

#include <QWidget>
#include <QMap>
#include <QPropertyAnimation>

class QtStackedWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int xPos READ Xpos WRITE setXPos)

public:
    enum MoveDirection { None, LeftDirection, RightDirection };

    explicit QtStackedWidget(QWidget *parent = nullptr);
    ~QtStackedWidget();

    // Compatibility with QStackedWidget
    int  addWidget(QWidget *widget);       // auto-index, returns index
    int  currentIndex() const { return m_nCurrentIndex; }
    int  count() const { return m_children.size(); }
    QWidget *currentWidget();

    void setPressMove(bool on) { m_bPressMove = on; }

signals:
    void currentChanged(int index);

public slots:
    void setCurrentIndex(int index);
    void setCurrentIndexNoAnim(int index);  // instant switch (login→dashboard)

private:
    void setXPos(int nValue);
    int  Xpos() { return m_nStartPos; }

    // Animation completion
    void finishMove();

private:
    bool    m_bPressMove;
    bool    m_bPressed;
    QPoint  m_startPos;

    QMap<int, QWidget*> m_children;
    int     m_nCurrentIndex;
    int     m_nNextIndex;

    int     m_nStartPos;
    int     m_nEndPos;
    int     m_nDirection;

    QPropertyAnimation *m_animation;
    bool    m_bRecovery;

protected:
    void resizeEvent(QResizeEvent *e) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
};

#endif // QTSTACKEDWIDGET_H
