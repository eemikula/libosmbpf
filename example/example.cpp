#include <iostream>

#include "libosmpbf.h"

int main(int argc, char *argv[]){

	if (argc != 2){
		std::cout << "Usage: " << argv[0] << " [FILE]\n";
		return 0;
	}

	libpbf::PbfStream pbf(argv[1]);
	if (!pbf){
		std::cout << "Not Ok\n";
		return 1;
	}

	libpbf::PbfBlock block;
	while (pbf >> block){

		for (libpbf::PbfBlock::NodeIterator i = block.nodesBegin(); i != block.nodesEnd(); i.next()){

			const libpbf::BlockNode &node = *i;
			const std::string *name = 0;
			bool restaurant = false;

			for (int key = 0; key < node.keys(); key++){
				libpbf::BlockTag pair = node.keys(key);

				if (pair.first == "amenity" && pair.second == "restaurant")
					restaurant = true;
				else if (pair.first == "name")
					name = &pair.second;
			}

			if (restaurant && name)
				std::cout << "Restaurant: " << *name << "\n";

		}

		for (libpbf::PbfBlock::WayIterator i = block.waysBegin(); i != block.waysEnd(); i.next()){

			const libpbf::BlockWay &way = *i;
			const std::string *name = 0;
			bool restaurant = false;

			for (int key = 0; key < way.keys(); key++){
				libpbf::BlockTag pair = way.keys(key);

				if (pair.first == "amenity" && pair.second == "restaurant")
					restaurant = true;
				else if (pair.first == "name")
					name = &pair.second;
			}

			if (restaurant && name)
				std::cout << "Restaurant: " << *name << "\n";

		}

	}

	return 0;
}
