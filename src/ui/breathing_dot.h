#pragma once

#include <QPropertyAnimation>
#include <QWidget>

class BreathingDot : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal dotOpacity READ getDotOpacity WRITE setDotOpacity)

public:
    explicit BreathingDot(QWidget* parent = nullptr);

    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    qreal getDotOpacity() const;
    void setDotOpacity(qreal opacity);

    qreal m_opacity = 1.0;
    bool m_active = false;
    QPropertyAnimation* m_animation = nullptr;
};
