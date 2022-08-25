#ifndef SYMBOL_FINDER_H
#define SYMBOL_FINDER_H

#include "library_info.h"

#include "library_base.h"

#include <QObject>

class SymbolFinder : public LibraryBase
{
Q_OBJECT

public:
	SymbolFinder( );
    virtual ~SymbolFinder() {}

	bool CheckSchematics();
	bool FixSymbols(QString const& folder);

Q_SIGNALS:


private:
	void SaveLibraryTable(QString const& fileName);

	void getProjectLibraries() override;
	void getGlobalLibraries() override;

	void CheckSchematic(QString const& schPath);
	void CreateSymbolList();

	bool HasSymbol(QString const& footprint) const;
	QStringList GetLegacySymbols(QString const& url) const;

	QStringList GetKicadSymbols(QString const& url) const;

	bool AttemptToFindSymbolPath(QString const& footprint, QString const& libraryPath );

	QMap<QString,QStringList> SymbolList;

	QStringList missingSymbolList;
	QStringList rescueSymbolList;
};

#endif