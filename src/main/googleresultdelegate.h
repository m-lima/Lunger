#ifndef GOOGLERESULTDELEGATE_H
#define GOOGLERESULTDELEGATE_H

#include <QStyledItemDelegate>

class GoogleResultDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit GoogleResultDelegate(QObject *parent = 0);

protected:
    void paint(QPainter *painter, QStyleOptionViewItem const &option, QModelIndex const &index) const;
    QSize sizeHint(QStyleOptionViewItem const &option, QModelIndex const &index) const;
    
};

#endif // GOOGLERESULTDELEGATE_H
