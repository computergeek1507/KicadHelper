#pragma once

#include <QString>

struct RefCompare
{
	bool operator()( QString const& a, QString const& b ) const;
	static bool Compare( QString const& a, QString const& b );

	static QString getNonInt(QString const& text);
	static int getInt(QString const& text);
};
