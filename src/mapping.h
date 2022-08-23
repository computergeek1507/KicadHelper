#pragma once

#include <QString>
#include <QJsonObject>

struct Mapping
{
	Mapping() = default;

	Mapping(QJsonObject const& json)
	{
		read(json);
	}

	Mapping(QString from_, QString to_ ):
		from(std::move(from_)),
		to( std::move( to_ ) )
	{
	}

	void write(QJsonObject& json) const
	{
		json["from"] = from;
		json["to"] = to;
	}

	void read(const QJsonObject& json)
	{
		from = json["from"].toString();
		to = json["to"].toString();
	}

	QString asString() const
	{
		return "\"" + from + "\",\"" + to + "\"";
	}

	QString from;
	QString to;
};