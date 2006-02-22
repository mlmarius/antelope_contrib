#include <algorithm>
#include <vector>
#include "seismicarray.h"
#include "Hypocenter.h"
#include "TimeWindow.h"
#include "StationChannelMap.h"
#include "dbpp.h"
#include "ensemble.h"
#include "seispp.h"
namespace SEISPP {
using namespace std;
using namespace SEISPP;

ThreeComponentEnsemble 
  *array_get_data(SeismicArray& stations,
	Hypocenter& hypo,string phase, 
        TimeWindow data_window, double tpad,
        DatascopeHandle& dbwf, 
	StationChannelMap& scmap,
	MetadataList& ensemble_mdl, MetadataList& member_mdl,
        AttributeMap& am)
{
	ThreeComponentEnsemble *result=NULL;
	StationTime times=ArrayPredictedArrivals(stations,hypo,phase);
	TimeWindow arrival_range=StationTimeRange(times);
	TimeWindow read_window(arrival_range.start-tpad+data_window.start,
                        arrival_range.end+tpad+data_window.end);
	TimeSeriesEnsemble rawdata;
	// We call this constructor to get all data AND require that site
	// and sitechan information will be loaded into Metadata area of 
	// each member of the ensemble.
	try {
		rawdata=TimeSeriesEnsemble(dbwf,read_window,
			"none","none",true,true,false);
	} catch (...) {throw;}
	// This procedure can be called because we can assume the data are now
	// segmented here.  i.e. each sta:chan defines a unique member
	StaChanSort(rawdata);
	//
	// Data are now assumed sorted by sta:chan.  This means we should be able
	// to bundle all data in groups of 3.  As we work through data we use
	// the StationChannelMap object to sort out the channel hierarchy.
	//
	try {
		// Use this constructor that clones ensemble metadata and 
		// set aside slots equal to rawdata.  Conservative, but
		// should not be that wasteful.
		result = new ThreeComponentEnsemble(dynamic_cast<Metadata&>(rawdata),
					rawdata.member.size()/3);
		string current_sta=rawdata.member[0].get_string("sta");
		ThreeComponentChannelMap tccm=scmap.channels(current_sta);
		vector<int> comp,prec,member_index;
		string chan;
		int i,j;
		for(i=0;i<rawdata.member.size();++i)
		{
			string nextsta=rawdata.member[i].get_string("sta");
			if(nextsta==current_sta)
			{
				try {
					chan=rawdata.member[i].get_string("chan");
					comp.push_back(tccm.component(chan));
					prec.push_back(tccm.precedence(chan));
					member_index.push_back(i);
				} 
				catch (MetadataError mde)
				{
					mde.log_error();
					throw SeisppError("array_get_data:  required attribute chan is not defined in input data");
				}
				catch (SeisppError sde)
				{
					cerr << "array_get_data:  Problem with member "
						<< i 
						<< "which contains data for station "<<current_sta
						<< " in raw input ensemble"<<endl;
					sde.log_error();
				}
			}
			else
			{
			//
			// Land here when a grouping is completed and we have
			// to sort out which channels are to be kept.
			// The components vector is used by the constructor
			// we use here to assemble pieces to make a 3c seis
			// The entire block is encased in this try block
			// mostly to trap errors in the ThreeComponentSeismogram
			// constructor.  In all cases errors in this block 
			// lead to dropped data.
			//
			    try{
				vector<TimeSeries> components;
				int ncomponents=comp.size();
				if(ncomponents==3)
				{
					//
					// This assumes the constructor below
					// will simply compute an appropriate
					// transformation matrix for the components
					// no matter what the order.  In fact with
					// standard SEED channel codes the
					// order will simply come out right 
					// automatically, but this still is a 
					// potential maintenance issue.
					//
					for(j=0;j<ncomponents;++j)
						components.push_back
						  (rawdata.member[member_index[j]]);
					ThreeComponentSeismogram new3c(components,0);
					result->member.push_back(new3c);
				}
				else if(ncomponents>3)
				{
				//
				// Land here when there are multiple channels
				// for each component direction (e.g. BHZ and LHZ)
				// We scan for the highest precedence number found
				// and then go back and extract components with that
				// precedence level.  If there are missing data 
				// for that set of components, delete those and 
				// keep trying until a complete set is found or 
				// the list is exhausted.
				//
					vector<int>::iterator minprec;
					minprec=min_element(prec.begin(),prec.end());
					for(j=0;j<comp.size();++j)
					{
						if(prec[j]==(*minprec))
							components.push_back
							  (rawdata.member[member_index[j]]);
					}
					do {
						if(comp.size()==3)
						{
							ThreeComponentSeismogram new3c(components);
							result->member.push_back(new3c);
						}
						else
						{
							int minval=*minprec;
							vector<int>::iterator ic,ip,im;
							for(ic=comp.begin(),
								ip=prec.begin(),
								im=member_index.begin();
								ic!=comp.end();
								++ic,++ip,++im)
							{
								if((*ip)==minval)
								{
									comp.erase(ic);
									prec.erase(ip);
									member_index.erase(im);
								}
							}
						}
					} while(comp.size()>3);
					//
					// This may be an unnecessary verbosity
					//
					if(comp.size()<3)
						cerr << "array_get_data_3c:  Irregular data from station "
							<< current_sta
							<< ".  Data from this station droped."
							<<endl;
				}
				else
				{
					// only land here if ncomponents<3 meaning bad data
					cerr << "Incomplete data for station "
						<< current_sta <<endl
						<< "Station has only "
						<< ncomponents
						<< " components and cannot be assembled into three component bundle"
						<< endl;
				}
			    } catch (SeisppError& serr)
			    {
				cerr << "Problems assembling data for station "
					<< current_sta << endl;
				serr.log_error();
				cerr << "Data from this station will be dropped"<<endl;
			    }
			}
		}
		return(result);
	}
	catch(...) {throw;}
}

} // End SEISPP Namespace declaration