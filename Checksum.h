//
// Created by andrea-pc on 02/11/20.
//

#ifndef PDSPROJECTSERVER_CHECKSUM_H
#define PDSPROJECTSERVER_CHECKSUM_H
#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <fstream>
#include <boost/crc.hpp>  // for boost::crc_32_type
#ifndef PRIVATE_BUFFER_SIZE
#define PRIVATE_BUFFER_SIZE  8192
#endif

// Global objects
std::streamsize const  buffer_size = PRIVATE_BUFFER_SIZE;

std::string calculate_checksum(std::ifstream &ifs) {

    std::string error = "Errore calcolo CRC";

    try
    {
        boost::crc_32_type  result;
        clock_t tStart = clock();
        if ( ifs )
        {
            do
            {
                char  buffer[ buffer_size ];

                ifs.read( buffer, buffer_size );
                result.process_bytes( buffer, ifs.gcount() );
            } while ( ifs );
        }
        else
        {
            std::cerr << "Impossibile aprire il file '"<< std::endl;
        }
        std::cout<<"Tempo impiegato per il calcolo del CRC: "<<(double)(clock() - tStart)/CLOCKS_PER_SEC;

        std::cout << std::hex << std::uppercase << result.checksum() << std::endl;

        std::stringstream stream;
        stream << std::hex << std::uppercase << result.checksum();
        std::string res = stream.str();
        return res;

    }
    catch ( std::exception &e )
    {
        std::cerr << "Found an exception with '" << e.what() << "'." << std::endl;
        return error;
        /* VA GESTITA LA RETURN ADATTA */
    }
    catch ( ... )
    {
        std::cerr << "Found an unknown exception." << std::endl;
        return error;
        /* VA GESTITA LA RETURN ADATTA */

    }

}
#endif //PDSPROJECTSERVER_CHECKSUM_H
