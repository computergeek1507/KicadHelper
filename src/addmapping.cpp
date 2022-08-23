#include "addmapping.h"

AddMapping::AddMapping(QWidget* parent) :
	ui(new Ui::AddMappingDialog)
{
	ui->setupUi(this);
}

AddMapping::~AddMapping()
{
	delete ui;
}

int AddMapping::Load()
{
	return this->exec();
}

std::pair<QString, QString> AddMapping::GetMapping()
{
	return { ui->leFromText->text() ,ui->leToText->text() };
}

void AddMapping::on_buttonBox_accepted()
{
	if (ui->leToText->text().isEmpty() || ui->leFromText->text().isEmpty())
	{
		return;
	}
	this->accept();
}