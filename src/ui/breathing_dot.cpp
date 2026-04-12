#include "ui/breathing_dot.h"

#include <QPainter>
#include <QRadialGradient>

BreathingDot::BreathingDot(QWidget* parent) : QWidget(parent) {
  setFixedSize(10, 10);
  m_animation = new QPropertyAnimation(this, "dotOpacity");
  m_animation->setDuration(1800);
  m_animation->setStartValue(0.3);
  m_animation->setEndValue(1.0);
  m_animation->setEasingCurve(QEasingCurve::InOutSine);
  m_animation->setLoopCount(-1);
}

void BreathingDot::setActive(bool active) {
  if (m_active == active)
    return;

  m_active = active;
  if (active) {
    m_animation->start();
  } else {
    m_animation->stop();
    m_opacity = 1.0;
  }

  update();
}

void BreathingDot::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QColor color =
      m_active ? QColor(34, 197, 94) : QColor(168, 162, 158);  // 绿色 / 灰色
  color.setAlphaF(m_active ? m_opacity : 0.6);

  if (m_active) {
    QRadialGradient glow(rect().center(), 7);
    QColor glowColor = color;
    glowColor.setAlphaF(m_opacity * 0.3);
    glow.setColorAt(0.0, glowColor);
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setBrush(glow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect().adjusted(-2, -2, 2, 2));
  }

  p.setBrush(color);
  p.setPen(Qt::NoPen);
  p.drawEllipse(rect());
}

qreal BreathingDot::getDotOpacity() const { return m_opacity; }

void BreathingDot::setDotOpacity(qreal opacity) {
  m_opacity = opacity;
  update();
}
