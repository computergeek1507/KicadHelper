#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <fstream>
#include <string>
#include <vector>

namespace csvparser
{
	std::vector<std::string> split(const std::string& s, char seperator)
	{
	   std::vector<std::string> output;

		std::string::size_type prev_pos {0};
		std::string::size_type pos {0};

		while((pos = s.find(seperator, pos)) != std::string::npos)
		{
			std::string substring( s.substr(prev_pos, pos-prev_pos) );

			output.push_back(substring);

			prev_pos = ++pos;
		}

		output.push_back(s.substr(prev_pos, pos-prev_pos)); // Last word

		return output;
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
			if(idx != std::string::npos )
			{
				line = line.substr (0, idx);
			}

			line.erase( std::remove_if( line.begin(), line.end(), isspace ), line.end() );
			auto lineVector = split( line, ',' );
			if(lineVector.size() != 0 )
			{
				lines.push_back(lineVector);
			}
		}
		return lines;
	}
}
#endif