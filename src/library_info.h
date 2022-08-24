#pragma once

#include <QString>

struct LibraryInfo
{
	LibraryInfo() = default;

	LibraryInfo(QString name_, QString type_, QString url_, QString options_, QString descr_) :
		name(std::move(name_)),
		type(std::move(type_)),
		url(std::move(url_)),
		options(std::move(options_)),
		descr(std::move(descr_))
	{
	}

	QString asString() const 
	{
		return QString("  (lib (name \"%1\")(type \"%2\")(uri \"%3\")(options \"%4\")(descr \"%5\"))").arg(name).arg(type).arg(url).arg(options).arg(descr);
	}

	QString name;
	QString type;
	QString url;
	QString options;
	QString descr;
};