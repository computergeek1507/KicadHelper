#pragma once

#include <QString>

struct PartInfo
{
	PartInfo() = default;


	PartInfo(QString value_, QString footPrint_,  QString digikey_, QString lcsc_, QString mpn_ ):
		value(std::move(value_)),
		footPrint( std::move( footPrint_ ) ),
		digikey( std::move( digikey_ ) ),
		lcsc( std::move( lcsc_ ) ),
		mpn( std::move( mpn_ ) )
	{
	}
	QString value;
	QString footPrint;
	QString digikey;
	QString lcsc;
	QString mpn;
};