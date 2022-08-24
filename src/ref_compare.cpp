#include "ref_compare.h"

bool RefCompare::operator()( QString const& a, QString const& b ) const
{
	return Compare(a, b);
}

bool RefCompare::Compare( QString const& a, QString const& b )
{
	int const ia {getInt(a)};
	int const ib {getInt(b)};
	QString const sa {getNonInt(a.toUpper())};
	QString const sb {getNonInt(b.toUpper())};

	if(sa == sb && ia != -1 && ib != -1 && !sa.isEmpty() && !sb.isEmpty()) 
	{
		return ia < ib;
	}

	if(!sa.isEmpty() && !sb.isEmpty()) 
	{
		return sa < sb;
	}
	return a < b;
}

QString RefCompare::getNonInt(QString const& text)
{
	QString letters;
	for(auto const& c : text)
	{
		if(c.isLetter()) 
		{
			letters.append(c);
		}
	}
	return letters;
}

int RefCompare::getInt(QString const& text)
{
	QString num;
	for(auto const& c : text)
	{
		if(c.isNumber())
		{
			num.append(c);
		}
	}
	return num.toInt();
}
