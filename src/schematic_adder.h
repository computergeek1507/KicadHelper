#ifndef SCHEMATIC_ADDER_H
#define SCHEMATIC_ADDER_H

#include "partinfo.h"

#include "spdlog/spdlog.h"

#include <QString>
#include <QObject>

class SchematicAdder : public QObject
{
Q_OBJECT

public:
	SchematicAdder( );
    ~SchematicAdder() {}

	bool AddPartNumbersToSchematics(QString const& schDir) const;
	void AddPart(QString value, QString footPrint,  QString digikey, QString lcsc, QString mpn );

	void ClearPartList(){ partList.clear(); }

Q_SIGNALS:
	void SendMessage( QString const& message, spdlog::level::level_enum llvl) const;

private:
	std::vector<PartInfo> partList;

	void UpdateSchematic(QString const& schPath) const;

};

#endif