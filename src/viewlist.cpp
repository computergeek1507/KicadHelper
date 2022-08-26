#include "viewlist.h"

ViewList::ViewList(QStringList const& listItems, QString const& header, QWidget* parent) :
	ui(new Ui::ViewListDialog)
{
	ui->setupUi(this);

	ui->listWidget->addItems(listItems);

	if (!header.isEmpty())
	{
		setWindowTitle(header);
	}
}

ViewList::~ViewList()
{
	delete ui;
}

bool ViewList::Load(QStringList const& listItems, QString const& header, QWidget* parent)
{
	ViewList vl(listItems, header, parent);
	return QDialog::Accepted == vl.exec();
}

