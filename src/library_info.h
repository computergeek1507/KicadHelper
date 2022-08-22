#pragma once

#include <QString>

struct LibraryInfo
{
	LibraryInfo() = default;

	LibraryInfo(QString name_, QString type_,  QString url_ ):
		name(std::move(name_)),
		type( std::move( type_ ) ),
		url( std::move( url_ ) )
	{
	}
	QString name;
	QString type;
	QString url;
};