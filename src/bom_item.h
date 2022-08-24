#pragma once

#include <QStringList>

#include "ref_compare.h"

struct BOMItem
{
	BOMItem() = default;

	BOMItem(QString value_, QString footPrint_, QString const& ref_, QString digikey_, QString lcsc_, QString mpn_ ):
		value(std::move(value_)),
		footPrint( std::move( footPrint_ ) ),
		digikey( std::move( digikey_ ) ),
		lcsc( std::move( lcsc_ ) ),
		mpn( std::move( mpn_ ) ),
		references(ref_),
		quantity(1)
	{
	}

	QString asString() const
	{
		auto refs{references};
		std::sort(refs.begin(),refs.end(), RefCompare());
		return "\"" + QString::number(quantity) +"\",\"" + refs.join(",") + "\",\"" + value + "\",\"" + digikey + "\",\"" + lcsc + "\",\"" + mpn + "\"";
	}

	QString value;
	unsigned int quantity;
	QString footPrint;
	QString digikey;
	QString lcsc;
	QString mpn;
	QStringList references;
};