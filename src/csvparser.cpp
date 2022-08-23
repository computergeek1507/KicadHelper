#include "csvparser.h"

#include <fstream>

namespace csvparser
{
	std::vector<std::string> readCSVRow(const std::string& row) {
		CSVState state = CSVState::UnquotedField;
		std::vector<std::string> fields{ "" };
		size_t i = 0; // index of the current field
		for (char c : row) {
			switch (state) {
			case CSVState::UnquotedField:
				switch (c) {
				case ',': // end of field
					fields.push_back(""); i++;
					break;
				case '"': state = CSVState::QuotedField;
					break;
				default:  fields[i].push_back(c);
					break;
				}
				break;
			case CSVState::QuotedField:
				switch (c) {
				case '"': state = CSVState::QuotedQuote;
					break;
				default:  fields[i].push_back(c);
					break;
				}
				break;
			case CSVState::QuotedQuote:
				switch (c) {
				case ',': // , after closing quote
					fields.push_back(""); i++;
					state = CSVState::UnquotedField;
					break;
				case '"': // "" -> "
					fields[i].push_back('"');
					state = CSVState::QuotedField;
					break;
				default:  // end of quote
					state = CSVState::UnquotedField;
					break;
				}
				break;
			}
		}
		return fields;
	}

	std::vector< std::vector< std::string > > parsefile(std::string const& path)
	{
		std::vector< std::vector< std::string > > lines;
		std::ifstream infile( path );
		if( !infile )
		{
			std::string err( "Error opening file " );
			err += path;
			throw std::runtime_error( err );
		}
		std::string line;
		while( std::getline( infile, line ) )
		{
			auto idx = line.find( '#' );
			if (infile.bad() || infile.fail()) {
				break;
			}
			if(idx != std::string::npos )
			{
				line = line.substr (0, idx);
			}

			//line.erase( std::remove_if( line.begin(), line.end(), isspace ), line.end() );
			auto lineVector = readCSVRow(line);
			if(lineVector.size() != 0 )
			{
				lines.push_back(lineVector);
			}
		}
		return lines;
	}
}
