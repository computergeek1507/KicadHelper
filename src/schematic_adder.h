#ifndef SCHEMATIC_ADDER_H
#define SCHEMATIC_ADDER_H

#include "partinfo.h"
#include "bom_item.h"

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
	void UpdatePart(QString const& value, QString const& fp, QString const& digi, QString const& lcsc, QString const& mpn, int index);

	//Import/Export Stuff
	void LoadJsonFile(const QString& jsonFile);
	void SaveJsonFile(const QString& jsonFile) const;
	void ImportPartNumerCSV(QString const& csvFile, bool overideParts);
	void ImportSchematicParts(QStringList const& schFiles, bool overideParts);
	void SavePartNumerCSV(QString const& fileName) const;

	void GenerateBOM(QString const& fileName, QString const& schDir);

	void ClearPartList(){ partList.clear(); }
	std::vector<PartInfo> const& getPartList() const { return partList; }

Q_SIGNALS:
	void SendMessage( QString const& message, spdlog::level::level_enum llvl, QString const& file) const;
	void RedrawPartList( bool save) const;
	void UpdatePartRow(int index) const;

private:
	std::vector<PartInfo> partList;
	std::vector<BOMItem> bomList;

	QRegularExpression propRx;
	QRegularExpression pinRx;

	void UpdateSchematic(QString const& schPath) const;
	void write(QJsonObject& json) const;
	void read(QJsonObject const& json);

	//BOM Stuff
	void ParseForBOM(QString const& fileName);
	void AddorUpdateBOM(QString value_, QString footPrint_, QString const& ref_, QString digikey_, QString lcsc_, QString mpn_);
	void SaveBOM(QString const& fileName);

};

#endif