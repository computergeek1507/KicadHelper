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
	void AddPart(PartInfo part );
	void RemovePart(int index);
	void LoadJsonFile(const QString& jsonFile);
	void SaveJsonFile(const QString& jsonFile);
	void ImportPartNumerCSV(QString const& csvFile);
	void ClearPartList(){ partList.clear(); }

	std::vector<PartInfo> const& getPartList() const { return partList; }

Q_SIGNALS:
	void SendMessage( QString const& message, spdlog::level::level_enum llvl) const;
	void RedrawPartList( bool save) const;

private:
	std::vector<PartInfo> partList;

	void UpdateSchematic(QString const& schPath) const;
	void write(QJsonObject& json) const;
	void read(QJsonObject const& json);
};

#endif