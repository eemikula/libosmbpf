#include <zlib.h>
#include <netinet/in.h>
#include <stdint.h>
#include <fstream>
#include <iostream>

#include "protobuf/osm.pb.h"
#include "libosmpbf.h"
using namespace libpbf;

Coords::Coords(){
	this->lat = this->lon = 0;
}

// lat/lon coordinates are stored in PBF files as integers, and getting lat/lon values
// requires conversion using the specified granularity of the relevant block
Coords::Coords(int64_t lat, int64_t lon, int32_t granularity){
	this->lat = lat*granularity/1000000000.0;
	this->lon = lon*granularity/1000000000.0;
}

Node::Node(){
	id = 0;
}

Node::Node(const BlockNode &n) : coords(n.coords()) {
	id = n.id();

	for (int i = 0; i < n.keys(); i++){
		BlockTag tag = n.keys(i);
		tags[tag.first] = tag.second;
	}
}

Way::Way(){
	id = 0;
}

Way::Way(const BlockWay &w){
	id = w.id();

	for (int i = 0; i < w.nodes(); i++){
		nodeIds.push_back(w.nodes(i));
	}

	for (int i = 0; i < w.keys(); i++){
		BlockTag tag = w.keys(i);
		tags[tag.first] = tag.second;
	}

}

Relation::Member::Member(uint64_t id, MemberType type, const std::string &r){
	this->id = id;
	this->type = type;
	this->role = r;
}

Relation::Relation(const BlockRelation &r){
	id = r.id();

	for (uint32_t i = 0; i < r.keys(); i++){
		BlockTag tag = r.keys(i);
		tags[tag.first] = tag.second;
	}

	for (uint32_t i = 0; i < r.members(); i++){
		BlockRelation::Member m = r.members(i);
		members.push_back(Member(m.id, m.type, m.role));
	}

}

BlockWay::BlockWay(const OSMPBF::Way &w, const OSMPBF::PrimitiveBlock &b) : way(w), block(b){

}

uint64_t BlockWay::id() const {return way.id();}

uint32_t BlockWay::keys() const {return way.keys_size();}

BlockTag BlockWay::keys(uint32_t i) const {
	unsigned int key = way.keys(i);
	unsigned int val = way.vals(i);
	return BlockTag(block.stringtable().s(key), block.stringtable().s(val));
}

uint32_t BlockWay::nodes() const {
	return way.refs_size();

}

uint64_t BlockWay::nodes(uint32_t i) const {
	int64_t node = 0;
	for (int n = 0; n <= i; n++)
		node += way.refs(n);
	return node;
}

BlockNode::BlockNode(const OSMPBF::PrimitiveBlock &b, int group, bool dense, int i, uint64_t idBase, uint64_t node) : block(b){
	this->group = group;
	this->dense = dense;
	this->i = i;
	this->idBase = idBase;
	this->node = node;
}

BlockNode::BlockNode(const OSMPBF::PrimitiveBlock &b, int group, int i) : block(b){
	this->group = group;
	this->dense = false;
	this->i = i;
	this->idBase = 0;
	this->node = 0;
}

uint64_t BlockNode::id() const {
	if (this->dense){
		const OSMPBF::DenseNodes &nodes = block.primitivegroup(this->group).dense();
		return this->idBase+nodes.id(this->node);
	} else {
		const OSMPBF::Node &n = block.primitivegroup(this->group).nodes(this->i);
		return n.id();
	}
}
unsigned int BlockNode::keys() const {
	if (this->dense){
		const OSMPBF::DenseNodes &nodes = this->block.primitivegroup(this->group).dense();
		unsigned int count = 0;
		for (int n = i; n < nodes.keys_vals_size() && nodes.keys_vals(n) != 0; n += 2, count++);
		return count;
	} else {
		const OSMPBF::Node &n = this->block.primitivegroup(this->group).nodes(this->i);
		return n.keys_size();
	}
}
BlockTag BlockNode::keys(uint64_t x) const {
	if (dense){
		const OSMPBF::DenseNodes &nodes = this->block.primitivegroup(this->group).dense();
		unsigned int key = nodes.keys_vals(this->i+x*2);
		unsigned int val = nodes.keys_vals(this->i+x*2+1);
		return BlockTag(this->block.stringtable().s(key), this->block.stringtable().s(val));
	} else {
		const OSMPBF::Node &n = block.primitivegroup(this->group).nodes(this->i);
		unsigned int key = n.keys(x);
		unsigned int val = n.vals(x);
		return BlockTag(this->block.stringtable().s(key), this->block.stringtable().s(val));
	}
}

Coords BlockNode::coords() const {
	if (dense){
		const OSMPBF::DenseNodes &nodes = this->block.primitivegroup(this->group).dense();
		int64_t lat = this->block.lat_offset(), lon = this->block.lon_offset();
		for (int x = 0; x <= node; x++){
			lat += nodes.lat(x);
			lon += nodes.lon(x);
		}
		return Coords(lat,lon,this->block.granularity());
	} else {
		const OSMPBF::Node &n = block.primitivegroup(this->group).nodes(this->i);
		return Coords(n.lat(),n.lon(),this->block.granularity());
	}
}

BlockRelation::BlockRelation(const OSMPBF::Relation &r, const OSMPBF::PrimitiveBlock &b) : relation(r), block(b){

}

uint64_t BlockRelation::id() const {
	return relation.id();
}

unsigned int BlockRelation::keys() const {
	return relation.keys_size();
}

BlockRelation::Member::Member(uint64_t id, MemberType type, const std::string &r) : role(r){
	this->id = id;
	this->type = type;
}

BlockTag BlockRelation::keys(uint32_t i) const {
	uint32_t key = relation.keys(i);
	uint32_t val = relation.vals(i);
	return BlockTag(block.stringtable().s(key), block.stringtable().s(val));
}

uint32_t BlockRelation::members() const {
	return relation.memids_size();
}

BlockRelation::Member BlockRelation::members(uint64_t i) const {

	uint64_t id = 0;
	for (int n = 0; n <= i; n++)
		id += relation.memids(n);

	if (relation.types(i) == OSMPBF::Relation_MemberType_NODE){
		return Member(id, Member_Node, block.stringtable().s(relation.roles_sid(i)));
	} else if (relation.types(i) == OSMPBF::Relation_MemberType_WAY){
		return Member(id, Member_Way, block.stringtable().s(relation.roles_sid(i)));
	} else if (relation.types(i) == OSMPBF::Relation_MemberType_RELATION){
		return Member(id, Member_Relation, block.stringtable().s(relation.roles_sid(i)));
	} else {
		//TODO: Throw exception
		return Member(0, Member_Node, "");
	}
}

PbfBlock::NodeIterator::NodeIterator(OSMPBF::PrimitiveBlock &b, bool end) : block(b){
	this->group = 0;
	this->i = 0;
	this->end = end;
	this->dense = true;
	this->idBase = 0;
	this->node = 0;

	if (!this->hasData())
		this->next();
}

bool PbfBlock::NodeIterator::hasData() const {
	return this->group < block.primitivegroup_size()
			&& (!this->dense && this->i < block.primitivegroup(this->group).nodes_size()
				|| (
					this->dense
					&& block.primitivegroup(this->group).has_dense()
					&& this->i < block.primitivegroup(this->group).dense().keys_vals_size()
				));
}

bool PbfBlock::NodeIterator::operator == (const PbfBlock::NodeIterator &i) const {
	if (this->end && i.end)
		return true;
	return this->end == i.end && this->i == i.i
		&& this->group == i.group && this->dense == i.dense
		&& this->node == i.node && this->idBase == i.idBase;
}

bool PbfBlock::NodeIterator::operator != (const PbfBlock::NodeIterator &i) const {
	return !(*this == i);
}

PbfBlock::NodeIterator &PbfBlock::NodeIterator::next(){

	if (this->end)
		return *this;

	do {
		if (this->dense && !block.primitivegroup(group).has_dense())
			this->dense = false;

		if (this->dense){

			const OSMPBF::DenseNodes &nodes = block.primitivegroup(group).dense();

			while (this->i < nodes.keys_vals_size() && nodes.keys_vals(i) != 0){
				this->i += 2;
			}

			if (this->i < nodes.keys_vals_size()-1){
				this->idBase += nodes.id(node);
				this->node++;
				// ensure i advances past the 0 delimiter
				this->i++;

			} else {
				this->dense = false;
				this->i = 0;
			}

		} else {
			if (this->i < block.primitivegroup(this->group).nodes_size()){
				this->i++;
			} else if (this->group < block.primitivegroup_size()-1){
				this->group++;
				this->i = 0;
				this->dense = true;
				this->node = 0;
				this->idBase = 0;
			} else {
				this->end = true;
			}
		}

	} while (!this->end && !this->hasData());

	return *this;
}

const BlockNode PbfBlock::NodeIterator::operator -> () const {
	if (this->dense){
		const OSMPBF::DenseNodes &nodes = block.primitivegroup(group).dense();
		return BlockNode(this->block, this->group, this->dense, this->i, this->idBase, this->node);
	} else {
		return BlockNode(this->block, this->group, this->i);
	}
}

const BlockNode PbfBlock::NodeIterator::operator * () const {
	if (this->dense){
		const OSMPBF::DenseNodes &nodes = block.primitivegroup(group).dense();
		return BlockNode(this->block, this->group, this->dense, this->i, this->idBase, this->node);
	} else {
		return BlockNode(this->block, this->group, this->i);
	}
}

PbfBlock::WayIterator::WayIterator(OSMPBF::PrimitiveBlock &b, bool end) : block(b){
	this->group = 0;
	this->i = 0;
	this->end = end;

	if (!this->hasData())
		this->next();
}

bool PbfBlock::WayIterator::hasData() const {
	return this->group < block.primitivegroup_size()
			&& this->i < block.primitivegroup(this->group).ways_size();
}

bool PbfBlock::WayIterator::operator == (const PbfBlock::WayIterator &i) const {
	if (this->end && i.end)
		return true;
	return this->end == i.end && this->i == i.i && this->group == i.group;
}

bool PbfBlock::WayIterator::operator != (const PbfBlock::WayIterator &i) const {
	return !(*this == i);
}

PbfBlock::WayIterator &PbfBlock::WayIterator::next(){

	if (this->end)
		return *this;

	do {

		if (this->i < block.primitivegroup(this->group).ways_size()){
			this->i++;
		} else if (this->group < block.primitivegroup_size()-1){
			this->group++;
			this->i = 0;
		} else {
			this->end = true;
		}

	} while (!this->end && !this->hasData());

	return *this;
}

const BlockWay PbfBlock::WayIterator::operator -> () const {
	return BlockWay(block.primitivegroup(this->group).ways(this->i), block);
}

const BlockWay PbfBlock::WayIterator::operator * () const {
	return BlockWay(block.primitivegroup(this->group).ways(this->i), block);
}

PbfBlock::RelationIterator::RelationIterator(OSMPBF::PrimitiveBlock &b, bool end) : block(b){
	this->group = 0;
	this->i = 0;
	this->end = end;

	if (!this->hasData())
		this->next();
}

bool PbfBlock::RelationIterator::hasData() const {
	return this->group < block.primitivegroup_size()
			&& this->i < block.primitivegroup(this->group).relations_size();
}

bool PbfBlock::RelationIterator::operator == (const PbfBlock::RelationIterator &i) const {
	if (this->end && i.end)
		return true;
	return this->end == i.end && this->i == i.i && this->group == i.group;
}

bool PbfBlock::RelationIterator::operator != (const PbfBlock::RelationIterator &i) const {
	return !(*this == i);
}

PbfBlock::RelationIterator &PbfBlock::RelationIterator::next(){

	if (this->end)
		return *this;

	do {

		if (this->i < block.primitivegroup(this->group).relations_size()){
			this->i++;
		} else if (this->group < block.primitivegroup_size()-1){
			this->group++;
			this->i = 0;
		} else {
			this->end = true;
		}

	} while (!this->end && !this->hasData());

	return *this;
}

const BlockRelation PbfBlock::RelationIterator::operator -> () const {
	return BlockRelation(block.primitivegroup(this->group).relations(this->i), block);
}

const BlockRelation PbfBlock::RelationIterator::operator * () const {
	return BlockRelation(block.primitivegroup(this->group).relations(this->i), block);
}

PbfBlock::PbfBlock(){
	block = new OSMPBF::PrimitiveBlock;
}

PbfBlock::~PbfBlock(){
	delete block;
}

int PbfBlock::granularity() const {return block->granularity();}

int PbfBlock::Nodes() const {
	return 0;
}

PbfBlock::NodeIterator PbfBlock::nodesBegin(){
	return NodeIterator(*block, false);
}

PbfBlock::NodeIterator PbfBlock::nodesEnd(){
	return NodeIterator(*block, true);
}

int PbfBlock::Ways() const {
	return 0;
}

PbfBlock::WayIterator PbfBlock::waysBegin(){
	return WayIterator(*block, false);
}

PbfBlock::WayIterator PbfBlock::waysEnd(){
	return WayIterator(*block, true);
}

int PbfBlock::Relations() const {
	return 0;
}

PbfBlock::RelationIterator PbfBlock::relationsBegin(){
	return RelationIterator(*block, false);
}

PbfBlock::RelationIterator PbfBlock::relationsEnd(){
	return RelationIterator(*block, true);
}

Node BlockNode::clone() const {
	return Node(*this);
}

Way BlockWay::clone() const {
	return Way(*this);
}

Relation BlockRelation::clone() const {
	return Relation(*this);
}

PbfStream::PbfStream(const char *file) : std::fstream(file){

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	OSMPBF::Blob blob;
	readBlob(*this, blob);

	if (*this){
		OSMPBF::HeaderBlock headerBlock;
		if (!getCompressedBlock(blob, headerBlock))
			this->setstate(std::ios_base::badbit);
		else {
			/*std::cout << "Required features:\n";
			for (int i = 0; i < headerBlock.required_features_size(); i++){
				std::cout << "\t" << headerBlock.required_features(i) << "\n";
			}*/
		}
	}
	
}

PbfStream::~PbfStream(){

}

std::fstream &PbfStream::operator >> (PbfBlock &block){

	OSMPBF::Blob blob;
	readBlob(*this, blob);
	
	if (!getCompressedBlock(blob, *block.block))
		this->setstate(std::ios_base::badbit);

	return *this;
}

std::fstream &PbfStream::readDataStr(std::fstream &in, std::string &str, size_t size){
	char *data = new char[size];
	in.read(data, size);
	str.assign(data, size);
	delete[] data;
	return in;
}

std::fstream &PbfStream::readBlob(std::fstream &in, OSMPBF::Blob &blob){
	OSMPBF::BlobHeader blobHeader;
	unsigned int blobHeaderSize;
	if (in.read((char*)&blobHeaderSize, sizeof(blobHeaderSize))){
		blobHeaderSize = ntohl(blobHeaderSize);
		try {
			std::string data;
			if (!readDataStr(in, data, blobHeaderSize)){
				in.setstate(std::ios_base::badbit);
				return in;
			}

			if (!blobHeader.ParseFromString(data))
				throw "Failed to parse from string";
			if (!readDataStr(in, data, blobHeader.datasize()))
				throw "Unable to read data";
			if (!blob.ParseFromString(data))
				throw "Failed to parse";

		} catch (const char *s){
			std::cerr << s << "\n";
			in.setstate(std::ios_base::badbit);
			return in;
		}
	}

	return in;
}

bool PbfStream::inflate(std::string data, unsigned char *buf, size_t bufSz){
	z_stream zstrm;
	zstrm.zalloc = Z_NULL;
	zstrm.zfree = Z_NULL;
	zstrm.opaque = Z_NULL;
	zstrm.avail_in = 0;
	zstrm.next_in = Z_NULL;
	if (inflateInit(&zstrm) != Z_OK){
		std::cerr << "Unable to initialize zlib\n";
		return false;
	}

	zstrm.avail_in = data.capacity();
	zstrm.next_in = (Bytef*)data.data();
	zstrm.avail_out = bufSz;
	zstrm.next_out = buf;
	int r = ::inflate(&zstrm, Z_NO_FLUSH);
	if (r == Z_STREAM_ERROR){
		std::cout << "Not OK: " << r << "\n";
		return false;
	}

	inflateEnd(&zstrm);
	return true;
}

template <typename T>
bool PbfStream::getCompressedBlock(OSMPBF::Blob &blob, T &block){
	try {
		if (blob.has_zlib_data()){

			unsigned char *buf = new unsigned char[blob.raw_size()];
			if (!inflate(blob.zlib_data(), buf, blob.raw_size()))
				throw "Unable to decompress zlib data";

			std::string datastr((char*)buf, blob.raw_size());
			if (!block.ParseFromString(datastr)){
				throw "Cannot parse block";
			}
			delete[] buf;
		}
	} catch (const char *s){
		std::cerr << s << "\n";
		return false;
	}

	return true;
}