#include "addpartnumber.h"

#include "partinfo.h"

AddPartNumber::AddPartNumber(QWidget* parent): 
	ui(new Ui::AddPartNumberDialog)
{
	ui->setupUi(this);
}

AddPartNumber::~AddPartNumber()
{
	delete ui;
}

int AddPartNumber::Load()
{
	return this->exec();
}

void AddPartNumber::on_buttonBox_accepted()
{
	if (ui->leValueText->text().isEmpty() || ui->leFootprintText->text().isEmpty())
	{
		return;
	}
	this->accept();
}

PartInfo AddPartNumber::GetPartInfo()
{
	return PartInfo(ui->leValueText->text(),
					ui->leFootprintText->text(), 
					ui->leDigikeyText->text(), 
					ui->leLCSCText->text(), 
					ui->leMPNText->text());
}