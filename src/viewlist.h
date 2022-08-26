#ifndef VIEW_LIST_H
#define VIEW_LIST_H

#include <QDialog>

#include "ui_viewlist.h"

class ViewList : public QDialog
{
	Q_OBJECT
public:

	static bool Load(QStringList const& listItems, QString const& header = QString(), QWidget* parent = nullptr);

protected:
	explicit ViewList(QStringList const& listItems, QString const& header, QWidget* parent);


public:
	~ViewList();


public Q_SLOTS:


private:
	Ui::ViewListDialog* ui;
};

#endif // VIEW_LIST_H
