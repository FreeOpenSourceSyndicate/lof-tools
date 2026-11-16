#include "gui/flow_layout.h"

#include <QWidget>

namespace loftools {

FlowLayout::FlowLayout(QWidget* parent, int margin, int h_spacing, int v_spacing)
    : QLayout(parent), h_space_(h_spacing), v_space_(v_spacing) {
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int h_spacing, int v_spacing)
    : h_space_(h_spacing), v_space_(v_spacing) {
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout() {
    while (QLayoutItem* item = takeAt(0)) delete item;
}

void FlowLayout::addItem(QLayoutItem* item) { items_.append(item); }

int FlowLayout::count() const { return items_.size(); }

QLayoutItem* FlowLayout::itemAt(int index) const {
    return items_.value(index);
}

QLayoutItem* FlowLayout::takeAt(int index) {
    if (index >= 0 && index < items_.size()) return items_.takeAt(index);
    return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const { return {}; }

bool FlowLayout::hasHeightForWidth() const { return true; }

int FlowLayout::heightForWidth(int width) const {
    return do_layout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    do_layout(rect, false);
}

QSize FlowLayout::sizeHint() const { return minimumSize(); }

QSize FlowLayout::minimumSize() const {
    QSize size;
    for (const QLayoutItem* item : items_)
        size = size.expandedTo(item->minimumSize());
    const QMargins m = contentsMargins();
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

int FlowLayout::do_layout(const QRect& rect, bool test_only) const {
    const QMargins m = contentsMargins();
    const QRect effective = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    int x = effective.x();
    int y = effective.y();
    int line_height = 0;

    for (QLayoutItem* item : items_) {
        const QSize hint = item->sizeHint();
        int next_x = x + hint.width() + h_space_;
        if (next_x - h_space_ > effective.right() + 1 && line_height > 0) {
            x = effective.x();
            y += line_height + v_space_;
            next_x = x + hint.width() + h_space_;
            line_height = 0;
        }
        if (!test_only) item->setGeometry(QRect(QPoint(x, y), hint));
        x = next_x;
        line_height = qMax(line_height, hint.height());
    }
    return y + line_height - rect.y() + m.bottom();
}

}  // namespace loftools
