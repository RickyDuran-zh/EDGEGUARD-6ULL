// QtStackedWidget implementation
// Horizontal swipe only — simplified from qt_demo

#include "qtstackedwidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

QtStackedWidget::QtStackedWidget(QWidget *parent) : QWidget(parent)
{
    m_bPressed = false;
    m_startPos = QPoint(0, 0);
    m_nCurrentIndex = 0;
    m_nNextIndex = -1;
    m_nStartPos = 0;
    m_nEndPos = 0;
    m_nDirection = None;
    m_bRecovery = false;
    m_bPressMove = true;

    m_animation = new QPropertyAnimation(this, "xPos");
    m_animation->setEasingCurve(QEasingCurve::OutQuad);
    m_animation->setDuration(150);
    connect(m_animation, &QPropertyAnimation::finished, this, &QtStackedWidget::finishMove);
}

QtStackedWidget::~QtStackedWidget()
{
    for (QWidget *w : m_children) {
        if (w) delete w;
    }
}

int QtStackedWidget::addWidget(QWidget *widget)
{
    if (!widget) return -1;
    int idx = m_children.size();
    widget->setParent(this);
    widget->setGeometry(0, 0, this->width(), this->height());
    widget->setVisible(idx == m_nCurrentIndex);
    m_children.insert(idx, widget);
    return idx;
}

QWidget *QtStackedWidget::currentWidget()
{
    return m_children.value(m_nCurrentIndex);
}

void QtStackedWidget::resizeEvent(QResizeEvent *e)
{
    for (QWidget *w : m_children)
        w->setFixedSize(e->size());
    QWidget::resizeEvent(e);
}

void QtStackedWidget::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_children.size() || index == m_nCurrentIndex)
        return;

    // Determine direction for animation
    int dir = (index > m_nCurrentIndex) ? LeftDirection : RightDirection;

    m_nNextIndex = index;
    m_nDirection = dir;

    for (QWidget *w : m_children)
        w->setVisible(false);

    m_children.value(m_nCurrentIndex)->setVisible(true);
    m_children.value(index)->setVisible(true);

    m_animation->setStartValue(0);
    m_nEndPos = (dir == LeftDirection) ? -this->width() : this->width();
    m_animation->setEndValue(m_nEndPos);
    m_animation->start();
}

void QtStackedWidget::setCurrentIndexNoAnim(int index)
{
    if (index < 0 || index >= m_children.size() || index == m_nCurrentIndex)
        return;

    for (QWidget *w : m_children)
        w->setVisible(false);

    m_nCurrentIndex = index;
    m_children.value(index)->setVisible(true);
    m_children.value(index)->move(0, 0);
    m_nNextIndex = -1;
    m_nDirection = None;
    m_nStartPos = 0;

    emit currentChanged(m_nCurrentIndex);
}

void QtStackedWidget::finishMove()
{
    for (QWidget *w : m_children)
        w->setVisible(false);

    if (!m_bRecovery && m_nNextIndex >= 0) {
        m_nCurrentIndex = m_nNextIndex;
    }

    m_children.value(m_nCurrentIndex)->setVisible(true);
    m_children.value(m_nCurrentIndex)->move(0, 0);
    m_nNextIndex = -1;
    m_nDirection = None;
    m_nStartPos = 0;
    m_bRecovery = false;

    emit currentChanged(m_nCurrentIndex);
}

void QtStackedWidget::setXPos(int nValue)
{
    m_nStartPos = nValue;

    if (LeftDirection == m_nDirection) {
        m_children.value(m_nCurrentIndex)->move(nValue, 0);
        if (m_nNextIndex >= 0)
            m_children.value(m_nNextIndex)->move(nValue + this->width(), 0);
    } else if (RightDirection == m_nDirection) {
        m_children.value(m_nCurrentIndex)->move(nValue, 0);
        if (m_nNextIndex >= 0)
            m_children.value(m_nNextIndex)->move(nValue - this->width(), 0);
    }
}

void QtStackedWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(this->rect(), QColor("#07111f"));  // match EdgeGuard bg
}

void QtStackedWidget::mousePressEvent(QMouseEvent *e)
{
    if (m_bPressMove && m_children.size() >= 2) {
        m_bPressed = true;
        m_startPos = e->pos();
    }
    QWidget::mousePressEvent(e);
}

void QtStackedWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_bPressMove && m_bPressed) {
        m_bPressed = false;
        if (None == m_nDirection) {
            // No accumulated movement — treat as a click, do nothing
            QWidget::mouseReleaseEvent(e);
            return;
        }

        m_animation->setStartValue(m_nStartPos);

        // 15% threshold to commit vs snap-back
        m_bRecovery = (qAbs(m_nStartPos) < this->width() * 0.15);

        if (LeftDirection == m_nDirection) {
            m_nEndPos = m_bRecovery ? 0 : -this->width();
        } else {
            m_nEndPos = m_bRecovery ? 0 : this->width();
        }

        m_animation->setEndValue(m_nEndPos);
        m_animation->start();
    }
    QWidget::mouseReleaseEvent(e);
}

void QtStackedWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_bPressed || !m_bPressMove) {
        QWidget::mouseMoveEvent(e);
        return;
    }

    m_nNextIndex = -1;
    int nXoffset = e->pos().x() - m_startPos.x();
    m_nDirection = (nXoffset < 0) ? LeftDirection : RightDirection;
    m_nStartPos += nXoffset;

    // Page 0 (LoginPage) and Page 1 (FaceLoginPage): absolutely no swipe
    if (0 == m_nCurrentIndex || 1 == m_nCurrentIndex) {
        m_nStartPos -= nXoffset;
        return;
    }
    // Page 2 (Dashboard): block right-swipe toward FaceLoginPage
    if (RightDirection == m_nDirection && 2 == m_nCurrentIndex) {
        m_nStartPos -= nXoffset;
        return;
    }
    // Last page: block left-swipe past end
    if (LeftDirection == m_nDirection && (m_children.size() - 1) == m_nCurrentIndex
        && qAbs(m_nStartPos) > this->width() * 0.1) {
        m_nStartPos -= nXoffset;
        return;
    }

    m_children.value(m_nCurrentIndex)->move(m_nStartPos, 0);

    if (m_children.value(m_nCurrentIndex)->x() < 0) {
        if (m_nCurrentIndex < (m_children.size() - 1)) {
            m_nNextIndex = m_nCurrentIndex + 1;
            m_children.value(m_nNextIndex)->setVisible(true);
            m_children.value(m_nNextIndex)->move(this->width() + m_nStartPos + 1, 0);
        }
    } else if (m_nCurrentIndex > 0) {
        m_nNextIndex = m_nCurrentIndex - 1;
        m_children.value(m_nNextIndex)->setVisible(true);
        m_children.value(m_nNextIndex)->move(m_nStartPos - 1 - this->width(), 0);
    }

    m_startPos = e->pos();
    QWidget::mouseMoveEvent(e);
}
