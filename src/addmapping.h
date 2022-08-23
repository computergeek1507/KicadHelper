#ifndef ADD_MAPPING_H
#define ADD_MAPPING_H

#include <QDialog>

#include "ui_addmapping.h"

class AddMapping : public QDialog
{
	Q_OBJECT

public:
	explicit AddMapping(QWidget* parent = nullptr);
	~AddMapping();

	int Load();
	std::pair<QString, QString> GetMapping();

public Q_SLOTS:
	void on_buttonBox_accepted();

private:
	Ui::AddMappingDialog* ui;
};

#endif // ADD_MAPPING_H
