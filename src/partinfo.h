#pragma once

#include <QString>

struct PartInfo
{
	PartInfo() = default;


	PartInfo( QString footPrint_, QString value_, QString digikey_, QString lcsc_, QString mpn_ ):
		footPrint( std::move( footPrint_ ) ),
		value( std::move( value_ ) ),
		digikey( std::move( digikey_ ) ),
		lcsc( std::move( lcsc_ ) ),
		mpn( std::move( mpn_ ) )
	{
	}

	QString footPrint;
	QString value;
	QString digikey;
	QString lcsc;
	QString mpn;
} ;