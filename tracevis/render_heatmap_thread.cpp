#include <stdafx.h>
#include "render_heatmap_thread.h"
#include "traceMisc.h"

void __stdcall heatmap_renderer::ThreadEntry(void* pUserData) {
	return ((heatmap_renderer*)pUserData)->heatmap_thread();
}

bool heatmap_renderer::render_graph_heatmap(thread_graph_data *graph)
{
	GRAPH_DISPLAY_DATA *linedata = graph->get_mainlines();
	if (!linedata || !linedata->get_numVerts())  return false;

	//build set of all heat values
	std::set<long> heatValues;
	map<std::pair<unsigned int, unsigned int>, edge_data>::iterator edgeit;
	map<std::pair<unsigned int, unsigned int>, edge_data>::iterator edgeEnd;
	
	graph->start_edgeD_iteration(&edgeit, &edgeEnd);
	for (; edgeit != edgeEnd; edgeit++)
		heatValues.insert(edgeit->second.weight);
	graph->stop_edgeD_iteration();

	int heatrange = heatValues.size();

	//create map of distances of each value in set, creating blue->red range
	map<long, int> heatDistances;
	set<long>::iterator setit;
	int distance = 0;
	for (setit = heatValues.begin(); setit != heatValues.end(); setit++)
	{
		heatDistances[*setit] = distance++;
	}

	int maxDist = heatDistances.size();
	map<long, int>::iterator distit = heatDistances.begin();
	map<long, COLSTRUCT> heatColours;
	
	int numColours = colourRange.size();
	heatColours[heatDistances.begin()->first] = *colourRange.begin();
	if (maxDist > 2)
	{
		for (std::advance(distit, 1); distit != heatDistances.end(); distit++)
		{
			float distratio = (float)distit->second / (float)maxDist;
			int colourIndex = floor(numColours*distratio);
			heatColours[distit->first] = colourRange[colourIndex];
		}
	}

	long lastColour = heatDistances.rbegin()->first;
	if (heatColours.size() > 1)
		heatColours[lastColour] = *colourRange.rbegin();

	unsigned int newsize = linedata->get_numVerts() * COLELEMS * sizeof(GLfloat) * 2;
	if (graph->heatmaplines->col_size() < newsize)
		graph->heatmaplines->expand(newsize * 2);

	GLfloat *ecol = graph->heatmaplines->acquire_col("3b");
	for (size_t i = 0; i < linedata->get_numVerts(); ++i) 
		ecol[i] = 1.0; //hello memset


	int newDrawn = 0;

	graph->start_edgeD_iteration(&edgeit, &edgeEnd);
	for (; edgeit != edgeEnd; edgeit++)
	{
		COLSTRUCT *edgecol = &heatColours[edgeit->second.weight];
		unsigned int vertIdx = 0;
		for (; vertIdx < edgeit->second.vertSize; vertIdx++)
		{
			unsigned int arraypos = edgeit->second.arraypos + vertIdx * 4;
			ecol[arraypos] = edgecol->r;
			ecol[arraypos + 1] = edgecol->g;
			ecol[arraypos + 2] = edgecol->b;
			ecol[arraypos + 3] = (float)1;
		}
		newDrawn++;
	}
	graph->stop_edgeD_iteration();

	if (newDrawn)
	{
		graph->heatmaplines->set_numVerts(linedata->get_numVerts());
		graph->needVBOReload_heatmap = true;
	}
	graph->heatmaplines->release_col();
	return true;
}

//convert 0-255 rgb to 0-1
float fcol(int value)
{
	return (float)value / 255.0;
}

//thread handler to build graph for each thread
//allows display in thumbnail style format
void heatmap_renderer::heatmap_thread()
{
	//allegro color keeps breaking here and driving me insane
	//hence own implementation
	colourRange.insert(colourRange.begin(), COLSTRUCT{ 0, 0, fcol(255) });
	colourRange.insert(colourRange.begin() + 1, COLSTRUCT{ fcol(105), 0,  fcol(255) });
	colourRange.insert(colourRange.begin() + 2, COLSTRUCT{ fcol(182), 0,  fcol(255) });
	colourRange.insert(colourRange.begin() + 3, COLSTRUCT{ fcol(255), 0, 0 });
	colourRange.insert(colourRange.begin() + 4, COLSTRUCT{ fcol(255), fcol(58), 0 });
	colourRange.insert(colourRange.begin() + 5, COLSTRUCT{ fcol(255), fcol(93), 0 });
	colourRange.insert(colourRange.begin() + 6, COLSTRUCT{ fcol(255), fcol(124), 0 });
	colourRange.insert(colourRange.begin() + 7, COLSTRUCT{ fcol(255), fcol(163), 0 });
	colourRange.insert(colourRange.begin() + 8, COLSTRUCT{ fcol(255), fcol(182), 0 });
	colourRange.insert(colourRange.begin() + 9, COLSTRUCT{ fcol(255), fcol(228 ), fcol(167)});

	while (!piddata || piddata->graphs.empty())
	{
		Sleep(200);
		continue;
	}

	while (true)
	{

		//only write we are protecting against happens while creating new threads
		//so not important to release this quickly
		if (!obtainMutex(piddata->graphsListMutex, "Heatmap Thread glm")) return;

		vector<thread_graph_data *> graphlist;
		map <int, void *>::iterator graphit = piddata->graphs.begin();
		for (; graphit != piddata->graphs.end(); graphit++)
			graphlist.push_back((thread_graph_data *)graphit->second);
		dropMutex(piddata->graphsListMutex, "Heatmap Thread glm");

		vector<thread_graph_data *>::iterator graphlistIt = graphlist.begin();
		while (graphlistIt != graphlist.end())
		{
			thread_graph_data *graph = *graphlistIt;
			render_graph_heatmap(graph);
			graphlistIt++;
			Sleep(80);
		}

		
		Sleep(HEATMAP_DELAY_MS);
	}
}

