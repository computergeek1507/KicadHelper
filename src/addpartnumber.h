#ifndef ADD_PART_NUMBER_H
#define ADD_PART_NUMBER_H

#include <QDialog>

#include "ui_addpartnumber.h"

struct PartInfo;

class AddPartNumber : public QDialog
{
	Q_OBJECT

public:
	explicit AddPartNumber(QWidget* parent = nullptr);
	~AddPartNumber();

	int Load();
	PartInfo GetPartInfo();

public Q_SLOTS:
	void on_buttonBox_accepted();

private:

	Ui::AddPartNumberDialog* ui;
};

#endif // ADD_PART_NUMBER_H