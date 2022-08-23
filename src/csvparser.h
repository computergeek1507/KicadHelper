#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <string>
#include <vector>

namespace csvparser
{
	//https://stackoverflow.com/questions/1120140/how-can-i-read-and-parse-csv-files-in-c
	enum class CSVState {
		UnquotedField,
		QuotedField,
		QuotedQuote
	};

	std::vector<std::string> readCSVRow(const std::string& row);

	std::vector< std::vector< std::string > > parsefile(std::string const& path);
};
#endif