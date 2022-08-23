#pragma once

#include <QString>
#include <QJsonObject>

struct PartInfo
{
	PartInfo() = default;

	PartInfo(QJsonObject const& json)
	{
		read(json);
	}

	PartInfo(QString value_, QString footPrint_,  QString digikey_, QString lcsc_, QString mpn_ ):
		value(std::move(value_)),
		footPrint( std::move( footPrint_ ) ),
		digikey( std::move( digikey_ ) ),
		lcsc( std::move( lcsc_ ) ),
		mpn( std::move( mpn_ ) )
	{
	}

	void write(QJsonObject& json) const
	{
		json["value"] = value;
		json["footPrint"] = footPrint;
		json["digikey"] = digikey;
		json["lcsc"] = lcsc;
		json["mpn"] = mpn;
	}

	void read(const QJsonObject& json)
	{
		value = json["value"].toString();
		footPrint = json["footPrint"].toString();
		digikey = json["digikey"].toString();
		lcsc = json["lcsc"].toString();
		mpn = json["mpn"].toString();
	}

	QString asString() const
	{
		return "\"" + value + "\",\"" + footPrint + "\",\"" + digikey + "\",\"" + lcsc + "\",\"" + mpn + "\"";
	}

	QString value;
	QString footPrint;
	QString digikey;
	QString lcsc;
	QString mpn;
};