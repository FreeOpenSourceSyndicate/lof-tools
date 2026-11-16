#ifndef LOFTOOLS_GUI_FLOW_LAYOUT_H
#define LOFTOOLS_GUI_FLOW_LAYOUT_H

#include <QLayout>
#include <QList>
#include <QRect>
#include <QSize>

namespace loftools {

// A layout that lays its items out left to right and wraps to a new row when
// the current row is full.  Used so the character editor's field groups split
// over more rows as the window narrows, instead of forcing a horizontal
// scrollbar.  A horizontal scrollbar only appears when a single item cannot fit
// on a row of its own (i.e. the viewport is narrower than that item's minimum
// width).
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = -1,
                        int h_spacing = -1, int v_spacing = -1);
    explicit FlowLayout(int margin = -1, int h_spacing = -1, int v_spacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;

private:
    int do_layout(const QRect& rect, bool test_only) const;

    QList<QLayoutItem*> items_;
    int h_space_;
    int v_space_;
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_FLOW_LAYOUT_H
