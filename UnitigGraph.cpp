#include "UnitigGraph.h"

UnitigGraph::UnitigGraph(deBruijnGraph& dbg)
{
	auto&& unbalanced = dbg.getJunctions();
	auto&& out_unbalanced = unbalanced.first;
	auto&& in_unbalanced = unbalanced.second;
	unsigned int index = 1;
	for (auto& v : out_unbalanced)
	{
		std::string curr = v.get_kmer();
		Vertex* source = dbg.getVertex(curr);
		index = connectUnbalanced(source, index, curr, dbg);
	}
	for (auto& w: in_unbalanced)
	{
		std::string curr = w.get_kmer();
		Vertex* sink = dbg.getVertex(curr);
		index = connectUnbalanced(sink, index, curr, dbg);
	}
	// DEBUG
	auto&& numV = boost::num_vertices(g_);
	auto&& numE = boost::num_edges(g_);
	std::cerr << "Unitig graph has " << numV << " vertices and " << numE << " edges" << std::endl;

	typedef std::map<UVertex, int> IndexMap;
	IndexMap mapIndex;
	boost::associative_property_map<IndexMap> propmapIndex(mapIndex);
	uvertex_iter vi, vi_end;
	int i = 1;
	const auto& name = boost::get(boost::vertex_name_t(),g_);
	std::cout << std::endl;
	for (boost::tie(vi,vi_end) = boost::vertices(g_); vi != vi_end; ++vi)
	{
		/*if (boost::in_degree(*vi,g_) == 0 or boost::out_degree(*vi,g_) == 0)
		{
			std::cerr << "Vertex " << i << std::endl;
			std::cerr << boost::get(name,*vi) << std::endl;
			std::cerr << "In: " << boost::in_degree(*vi,g_) << " Out: " << boost::out_degree(*vi,g_) << std::endl;
		}*/
		boost::put(propmapIndex,*vi,i++);
	}
	boost::write_graphviz(std::cout, g_, boost::make_label_writer(boost::get(boost::vertex_name_t(),g_)), boost::make_label_writer(boost::get(boost::edge_name_t(),g_)), boost::default_writer(), propmapIndex);
}

UVertex UnitigGraph::addVertex(unsigned int index, std::string name)
{
	UVertex uv = boost::add_vertex(g_);
	boost::property_map<UGraph, boost::vertex_name_t>::type n = boost::get(boost::vertex_name_t(), g_);
	boost::put(n, uv, name);
	boost::property_map<UGraph, boost::vertex_index1_t>::type idx = boost::get(boost::vertex_index1_t(), g_);
	boost::put(idx, uv, index);
	auto&& ins = std::make_pair(index,uv);
	graph_.insert(ins);
	return uv;
}

unsigned int UnitigGraph::connectUnbalanced(Vertex* source, unsigned int index, std::string curr, deBruijnGraph& dbg)
{
	std::vector<char> succ = source->get_successors();
	std::vector<char> pred = source->get_predecessors();
	UVertex uv;
	if (!source->is_visited())
	{
		uv = addVertex(++index, curr);
		source->set_index(index);
		//dfs for all neighbours
	}
	else
	{
		unsigned int idx = source->get_index();
		uv = graph_[idx];
	}
	source->visit();
	index = addNeighbours(curr, succ, pred, dbg, index, uv);

	return index;
}

unsigned int UnitigGraph::addNeighbours(std::string& curr, const std::vector<char>& succ, const std::vector<char>& pred, deBruijnGraph& dbg, unsigned int index, UVertex& uv)
{
	bool rc = false;
	for (const auto& n : succ)
	{
		std::string sequence("");
		std::string next = curr.substr(1) + n;
		auto&& nextV = dbg.getVertex(next);
		if (!nextV)
		{
			rc = true;
			break;
		}
		else
			rc = false;
		// this vertex has been found from a complement
		sequence += n;
		if (!buildEdge(uv, nextV, next, sequence, index, dbg))
			index++;
	}
	if (rc)
	{
		for (const auto& n : pred)
		{
			std::string sequence("");
			std::string next = curr.substr(1) + deBruijnGraph::complement(n);
			auto&& nextV = dbg.getVertex(next);
			//this vertex has been found from a complement
			sequence += deBruijnGraph::complement(n);
			if (!buildEdge(uv, nextV, next, sequence, index, dbg))
				index++;
		}
	}
	return index;
}

bool UnitigGraph::buildEdge(UVertex src, Vertex* nextV, std::string next, std::string& sequence, unsigned int index, deBruijnGraph& dbg)
{
	bool visited = true;
	// with a little effort this can be moved inside the while loop for efficiency reasons
	auto&& succ = nextV->get_successors();
	auto&& pred = nextV->get_predecessors();
	while (!nextV->is_visited() and succ.size() == 1 and pred.size() == 1)
	{
		nextV->visit();
		char c = succ[0];
		Sequence tmp = dbg.getSequence(next);
		if (tmp == next)
			next = next.substr(1) + c;
		else
		{
			c = pred[0];
			next = next.substr(1) + deBruijnGraph::complement(c);
		}
		nextV = dbg.getVertex(next);
		pred = nextV->get_predecessors();
		succ = nextV->get_successors();
		sequence += c; 
	}
	/* 
	if nextV is visited then nextV may either be a junction, in which case it should have been
	added as a vertex to the graph and will receive an edge. Or the path we are starting to build has already
	been found having the target of the path to be found as source. This means we can break now.
	If nextV still isn't visited we found a junction which has not been considered before
	*/
	if (!nextV->is_visited())
	{
		nextV->visit();
		addVertex(++index, next);
		nextV->set_index(index);
		visited = false;
	}
	else if (succ.size() == 1 and pred.size() == 1)
		return true; // path has been found, do not add anything
	UEdge e = (boost::add_edge(src, graph_[nextV->get_index()],g_)).first;
	boost::property_map<UGraph, boost::edge_name_t>::type name = boost::get(boost::edge_name_t(), g_);
	boost::put(name,e,sequence);
	return visited;
}
