#include <stdio.h>
#include "elog.h"
#include "dbpp.h"
#include "seispp.h"
#include "SeisppError.h"
using namespace std;
using namespace SEISPP;
namespace SEISPP
{

// Simple function needed by constructors below.  Returns true if
// the string dbgroup is found in the command to be send to dbprocess.
// Throws an int exception if the dbgroup clause is anywhere but 
// the last line
bool dbgroup_used(Tbl *t)
{
	string s;
	int ierr;
	int n=maxtbl(t);
	for(int i=0;i<n;++i)
	{
		s = string(static_cast<char *>(gettbl(t,i)));
		ierr=s.find("dbgroup");
		if(ierr>=0)
		{
			if(i==(n-1))
				return true;
			else
				throw 1;
		}
	}
	return false;
}
/* Tricky function to test if an Antelope Dbptr is a bundle 
pointer.  

Function returns true of the Dbptr is the result of dbgroup.
Returns false otherwise.
*/
bool is_a_bundle_pointer(Dbptr db)
{
	Dbptr dbtest;
	dbtest=dblookup(db,0,0,"bundle",0);
	if(dbtest.field<0)
		return false;
	else
		return true;
}
bool is_view_test(Dbptr db)
{
	int is_view;
	dbquery(db,dbTABLE_IS_VIEW,&is_view);
	if(is_view)
		return true;
	else
		return false;
}
// Default constructor needs to initialize the pointer to 
// mark it as invalid
DatascopeHandle::DatascopeHandle()
{
	db.database=dbINVALID;
	db.table=dbINVALID;
	db.field=dbINVALID;
	db.record=dbINVALID;
	is_bundle=false;
	close_on_destruction=false;
	parent_table.database=dbINVALID;
	parent_table.table=dbINVALID;
	parent_table.field=dbINVALID;
	parent_table.record=dbINVALID;
	views=NULL;
	retain_parent=false;
}
	
/* dbprocess driven constructor for an Antelope_Database (handle) object.
	dbname = name of database to be openned (always openned r+)
	pf = pf (parameter file object) pointer containing dbprocess list
	tag = name of Tbl in pf for list of commands passed to dbprocess.
Will throw a SeisppDberror if problems happen in dbprocess, pf routines,
or in dbprocess.  This is an example of a constructor that acquires
a resource (in this case valid pointers to a view into a database)
*/
DatascopeHandle::DatascopeHandle(string dbname,
	string pfname, 
		string tag,
			bool readonly,
				bool retain_all_views)
{
	if(readonly)
	{
		if(dbopen(const_cast<char*>(dbname.c_str()),"r",&db))
			throw SeisppDberror("Failure in dbopen",db,complain);
	}
	else
	{
		if(dbopen(const_cast<char*>(dbname.c_str()),"r+",&db))
			throw SeisppDberror("Failure in dbopen",db,complain);
	}
	close_on_destruction=true;
	retain_parent=retain_all_views;
	// Probably needs to be an auto_ptr so pffree can be called
	// Maybe should use a Metadata object
	Pf *pf;
	if(pfread(const_cast<char*>(pfname.c_str()),&pf))
		throw SeisppError("Failure in pfread");
	Tbl *process_list;
	process_list = pfget_tbl(pf,const_cast<char*>(tag.c_str()));
	if(process_list==NULL)
	{
		freetbl(process_list,0);
		throw SeisppError("Tag name = "+tag+" not in parameter file");
	}
	try {
		is_bundle = dbgroup_used(process_list);
	} catch (...)
	{
		freetbl(process_list,0);
		throw SeisppError("Error in process list specification:  dbgroup can only be used as last command");
	}
	db = dbprocess(db,process_list,0);
	freetbl(process_list,0);
	if(db.table == dbINVALID)
		throw SeisppDberror("dbprocess failed",db,complain);
	pffree(pf);
	/* initialize views */
	views = new multiset<int>;
	bool is_view=is_view_test(db);
	if(is_view)
	{
		views->insert(db.table);
	}
	// Always initialize -- better than garbage
	parent_table=db;
	if(is_bundle) --parent_table.table;
	close_on_destruction=false;
}
/* Construct from a raw pointer.  See dbpp.h or doxygen pages
for more details */
DatascopeHandle::DatascopeHandle(Dbptr dbi, Pf *pf, string tag,
			bool manage_memory,bool retain_all_views)
{
	Tbl *process_list;
	process_list = pfget_tbl(pf,const_cast<char*>(tag.c_str()));
        if(process_list==NULL)
                throw SeisppError("Tag name = "+tag+" not in parameter file");
	try {
		is_bundle = dbgroup_used(process_list);
	} catch (...)
	{
		throw SeisppError("Error in process list specification:  dbgroup can only be used as last command");
	}
        db = dbprocess(dbi,process_list,0);
        if(db.table == dbINVALID)
                throw SeisppDberror("dbprocess failed",db,complain);
	if(manage_memory)
	{
		/* initialize views */
		views = new multiset<int>;
		bool is_view=is_view_test(db);
		if(is_view) views->insert(db.table);
	}
	else
		views=NULL;
	// Always initialize -- better than garbage
	parent_table=db;
	if(is_bundle) --parent_table.table;
	close_on_destruction=false;
	freetbl(process_list,0);
	retain_parent=retain_all_views;
}

// Plain Jane version just opens the database and sets the Dbptr
DatascopeHandle::DatascopeHandle(string dbname,bool readonly,
		bool retain_all_views)
{
	if(readonly)
	{
		if(dbopen(const_cast<char*>(dbname.c_str()),"r",&db))
			throw SeisppDberror("Failure in dbopen",db,complain);
	}
	else
	{
		if(dbopen(const_cast<char*>(dbname.c_str()),"r+",&db))
			throw SeisppDberror("Failure in dbopen",db,complain);
	}
	is_bundle = false;
	close_on_destruction=false;
	views = new multiset<int>;
	retain_parent=retain_all_views;
}
// copy constructor 
DatascopeHandle::DatascopeHandle(const DatascopeHandle& dbi)
{
	db = dbi.db;
	is_bundle=dbi.is_bundle;
	close_on_destruction=false; // copies should be this by default
	parent_table=dbi.parent_table;
	/* Note this is a pointer so this creates a link back to 
	the original.  The destructor has to manage this memory
	and this provides a clean way to pass a common area around
	between copies.  */
	views=dbi.views;
	views->insert(db.table);
	retain_parent=dbi.retain_parent;
/*
cerr << "DEBUG:  table "<<db.table<<" has views->count="<<views->count(db.table)<<endl;
*/
}
/* An odd specialized copy constructor for Datascope db.  
Handle is constructed using an existing db pointer, dbi, 
and setting the "parent" for this handle using dbip.  
A special problem is that the function has to do an independent
test to see if dbi is a bundle pointer or not to set the is_bundle
variable.  This is necessary as Datascope has no way to independently
tell if a Dbptr is a bundle or just a plain table or view.  
*/

DatascopeHandle::DatascopeHandle(Dbptr dbi,Dbptr dbip)
{
	db = dbi;
	parent_table = dbip;
	is_bundle=is_a_bundle_pointer(dbi);
	close_on_destruction=false; // copies should be this by default
	/* This is an implicit signal to this object that it should
	not manage the memory on this object.  The copy created
	through this mechanism will leak memory if used to create
	any views. */
	views=NULL;
	/* This is the only case that sets this true by default.  Assume
	that if this constructor is called you don't care about memory
	leaks anyway.  */
	retain_parent=true; 
}
/* Destructor.  This is fairly complex because the multiset 
container is used to manage memory.  See dbpp.h and doxygen
pages or details */
	
DatascopeHandle::~DatascopeHandle()
{
	if(close_on_destruction)
	{
		if(views!=NULL) delete views;
		dbclose(db);
	}
	else if (views!=NULL)
	{
		/* do nothing here unless this is a view.  If it
		is a view call dbfree only if this is the last 
		handle pointing to that view */
		bool is_view=is_view_test(db);
		if(is_view) 
		{
//cerr << "DEBUG:  views->count(): "<<views->count(db.table)<<" for table "<<db.table<<endl;
			int viewcount=views->count(db.table);
			if((viewcount<=0) && SEISPP_verbose)
			{
			   cerr << "DatascopeHandle destructor:  "
				<< "Coding error.  Destructor called when"
				<< " view count for view number "
				<< db.table << " was zero."<<endl;
			}
			else
			{
			  multiset<int>::iterator viewsptr;
			  viewsptr=views->find(db.table);
			  /* Do not test viewsptr as we can't get here
			  if db.table is now found in the multiset */
			  views->erase(viewsptr);
			  int viewcount=views->count(db.table);
//cerr << "DEBUG:  after erase views->count(): "<<viewcount<<" for table "<<db.table<<endl;
			  if(viewcount==1)
			  {
//cerr << "DEBUG: Calling dbfree on table " << db.table <<endl;
			    int testfree=dbfree(db);
			    if(testfree==dbINVALID)
				throw SeisppError(string("DatascopeHandle Destructor:")
				 + string(" dbfree returned dbINVALID.") );
			  }
/*
else
cerr << "DEBUG:  dbfree not called"<<endl;
*/
			}
		}
	}
}
// function that needs to be called to force closing database
void DatascopeHandle::close()
{
	close_on_destruction=true;
}
double DatascopeHandle::get_double(string name)
{
	double val;
	if(dbgetv(db,0,name.c_str(),&val,0))
		throw SeisppDberror(string("dbgetv error extracting attribute ")
			+name,
			db,complain);
	return(val);
}
int DatascopeHandle::get_int(string name)
{
	int val;
	if(dbgetv(db,0,name.c_str(),&val,0))
		throw SeisppDberror(string("dbgetv error extracting attribute ")
			+name,
			db,complain);
	return(val);
}
string DatascopeHandle::get_string(string name)
{
	char val[128];
	if(dbgetv(db,0,name.c_str(),&val,0))
		throw SeisppDberror(string("dbgetv error extracting attribute ")
			+name,
			db,complain);
	return(string(val));
}
string  DatascopeHandle::get_filename()
{
	char str[256];

	if(dbgetv(db,0,"dir",str,0) )
		throw SeisppDberror("dbgetv error trying to read dir",db,complain);
	string dir(str);
	if(dbgetv(db,0,"dfile",str,0) )
		throw SeisppDberror("dbgetv error trying to read dfile",db,complain);
	return(dir+"/"+string(str));
}
	
int DatascopeHandle::number_tuples()
{
	int ierr;
	int nrec;
	ierr = dbquery(db,dbRECORD_COUNT,&nrec);
	if(ierr<0)
		throw SeisppDberror(string("dbquery of record count failed"),
			db,complain);
	return(nrec);
}
int DatascopeHandle::number_attributes()
{
	int ierr;
	int natt;
	ierr = dbquery(db,dbFIELD_COUNT,&natt);
	if(ierr<0)
		throw SeisppDberror(string("number_attributes:  dbquery failed"),
			db,complain);
	return(natt);
}
DatascopeHandle& DatascopeHandle::operator=(const DatascopeHandle& dbi)
{
	if(this!=&dbi)
	{
		db=dbi.db;
		is_bundle=dbi.is_bundle;
		close_on_destruction=false;
		parent_table=dbi.parent_table;
		views=dbi.views;
		views->insert(db.table);
	}
//cerr << "DEBUG:  table "<<db.table<<" has views->count="<<views->count(db.table)<<endl;
	return(*this);
}
void DatascopeHandle::operator ++()
{
	++db.record;
}
// add row function.  Adds a blank row to db and returns the
// record number of the new row.  Assumes Dbptr points at
// one table.  Throws an exception if the add fails

int DatascopeHandle::append()
{
	int row_added;
	row_added = dbaddnull(db);
	if(row_added==dbINVALID)
		throw SeisppDberror(string("DatascopeHandle::append: dbaddnull failure"),
			db,complain);
	db.record=row_added;
	return(row_added);
}
// Put functions.  All can have the same name because of overloading
// in all put functions first arg is the attribute name and the second
// is the value to be deposited in the current row.
// I could have used a template except some versions require a cast
// Maybe there is a way around that but I don't know it
void DatascopeHandle::put(string name, double value)
{
	if(dbputv(db,0,name.c_str(),value,0) == dbINVALID)
		throw SeisppDberror(string("dbputv error with attribute "+name),
				db, complain);
}
void DatascopeHandle::put(string name, float value)
{
	if(dbputv(db,0,name.c_str(),static_cast<double>(value),0) == dbINVALID)
		throw SeisppDberror(string("dbputv error with attribute "+name),
				db, complain);
}
void DatascopeHandle::put(string name, int value)
{
	if(dbputv(db,0,name.c_str(),value,0) == dbINVALID)
		throw SeisppDberror(string("dbputv error with attribute "+name),
				db, complain);
}
void DatascopeHandle::put(string name, string value)
{
	if(dbputv(db,0,name.c_str(),value.c_str(),0) == dbINVALID)
		throw SeisppDberror(string("dbputv error with attribute "+name),
				db, complain);
}
void DatascopeHandle::put(string name, char *value)
{
	if(dbputv(db,0,name.c_str(),value,0) == dbINVALID)
		throw SeisppDberror(string("dbputv error with attribute "+name),
				db, complain);
}
void DatascopeHandle::put(string name, const char *value)
{
	if(dbputv(db,0,name.c_str(),const_cast<char *>(value),0) == dbINVALID)
		throw SeisppDberror(string("dbputv error with attribute "+name),
				db, complain);
}
Tbl *list_to_tbl(list<string> slist)
{
	list<string>::iterator sl;
	Tbl *t;
	t=newtbl(slist.size());
	for(sl=slist.begin();sl!=slist.end();++sl)
	{
		char *s;
		s = strdup((*sl).c_str());
		pushtbl(t,s);
	}
	return(t);
}
	
void DatascopeHandle::sort(list<string> sortkeys)
{
	Tbl *t;
	t = list_to_tbl(sortkeys);
	parent_table=db;
	db = dbsort(db,t,0,0);
	if(!retain_parent) manage_parent();
	if(db.table == dbINVALID)
		throw SeisppDberror("dbsort failed",db,complain);
	if(views!=NULL) views->insert(db.table);
	freetbl(t,free);
}
	
// natural join requires no join keys
void DatascopeHandle::natural_join(string table1, string table2)
{
	Dbptr dbj1, dbj2;
	dbj1 = dblookup(db,0,const_cast<char *>(table1.c_str()),0,0);
	dbj2 = dblookup(db,0,const_cast<char *>(table2.c_str()),0,0);
	db = dbjoin(dbj1, dbj2,0,0,0,0,0);
	if(db.table==dbINVALID)
		throw SeisppDberror(string("dbjoin of tables ")
			+ table1 
			+ string(" and ") 
			+ table2 
			+ string("failed"),
			db,complain);
	if(views!=NULL) views->insert(db.table);
}
// special case appending to current db
void DatascopeHandle::natural_join(string table)
{
	parent_table=db;
	db = dbjoin(db,dblookup(db,0,const_cast<char *>(table.c_str()),0,0),
		0,0,0,0,0);
	if(db.table==dbINVALID)
		throw SeisppDberror(string("dbjoin: append to current view with table") 
			+ table 
			+ string("failed"),
			db,complain);
	if(!retain_parent) manage_parent();
	if(views!=NULL) views->insert(db.table);
}

// full case with different lists for table 1 and table 2
void DatascopeHandle::join(string table1, string table2, 
	list<string> joinkeys1, list<string> joinkeys2)
{
	Tbl *t1,*t2;
	t1 = list_to_tbl(joinkeys1);
	t2 = list_to_tbl(joinkeys2);
	Dbptr dbj1, dbj2;
	dbj1 = dblookup(db,0,const_cast<char *>(table1.c_str()),0,0);
	dbj2 = dblookup(db,0,const_cast<char *>(table2.c_str()),0,0);
	db = dbjoin(dbj1, dbj2,&t1,&t2,0,0,0);
	freetbl(t1,free);
	freetbl(t2,free);
	if(db.table==dbINVALID)
		throw SeisppDberror(string("dbjoin of tables ")
			+ table1 
			+ string(" and ") 
			+ table2 
			+ string("failed"),
			db,complain);
	if(views!=NULL) views->insert(db.table);
}

void DatascopeHandle::group(list<string> groupkeys)
{
	Tbl *t;
	t = list_to_tbl(groupkeys);
	/* Note group must not manage it's parent as the idea of
	a group is irrational if the parent is destroyed. */
	parent_table=db;
	db = dbgroup(db,t,0,0);
	is_bundle=true;
	freetbl(t,free);
	if(db.table==dbINVALID)
		throw SeisppDberror(string("dbgroup failed"),
			db,complain);
	if(views!=NULL) views->insert(db.table);
}
void DatascopeHandle::subset(string sstr)
{
	parent_table=db;
	db = dbsubset(db,const_cast<char *>(sstr.c_str()),0);
	if(db.table==dbINVALID)
		throw SeisppDberror(string("dbsubset failed"),
			db,complain);
	if(!retain_parent) manage_parent();
	if(views!=NULL) views->insert(db.table);
}
void DatascopeHandle::lookup(string t)
{
	db = dblookup(db,0,const_cast<char *>(t.c_str()),
		0,0);
	if(db.table==dbINVALID)
		throw SeisppDberror(string("lookup:  lookup failed for table"
			+ t),db,complain);
	// Looking up a table forces this to be valid.
	// Problematic for a named view, but I'm not supporting
	// that concept in this interface anyway.
	is_bundle=false;
}
DBBundle DatascopeHandle::get_range()
{
	DBBundle bundle;
	int s,e;
	const string emess("get_range:  handle is not a bundle pointer");
	if(is_bundle)
	{
		Dbptr dbbundle;
		int ierr;
		ierr = dbgetv(db,0,"bundle",&dbbundle,0);
		if(ierr==dbINVALID)
			throw SeisppDberror(emess,db,complain);
		dbget_range(dbbundle,&s,&e);
		bundle.start_record = s;
		bundle.end_record = e;
		bundle.parent = dbbundle;
	}
	else
		throw SeisppDberror(emess,db,complain);
	return(bundle);
}


// error object functions 
SeisppDberror::SeisppDberror(const string mess, Dbptr dbi)
		: SeisppError(mess)
{
	db=dbi;
	error_type=unknown;
}
SeisppDberror::SeisppDberror(const string mess, Dbptr dbi,
		ErrorSeverity et) : SeisppError(mess)
{
	db=dbi;
	error_type=et;
}

void SeisppDberror::log_error()
{
	cerr<<"Database related error:"<<endl
		<<message<<endl
	<< "Current database pointer:"
	<<"Current database pointer:"
	<<"Database="<<db.database
	<<", Table="<<db.table
	<<", Record="<<db.record
	<<", Field="<<db.field<<endl
	<<"severity level=";
	switch (error_type)
	{
	case fault:
		cerr << " Fault" << endl;
		break;
	case fatal:
		cerr << " Fatal" << endl;
		break;
	case complain:
		cerr << " Complain" << endl;
		break;
	case notify:
		cerr << " Notify" << endl;
		break;
	case log:
		cerr << " Log" << endl;
		break;
	default:
		cerr << " Unknown" << endl;
	}
	clear_register(1);
}
/* Private method used by relational routines. */
void DatascopeHandle::manage_parent()
{
	bool is_view;
	is_view = is_view_test(parent_table);
	/* Note this effectively does nothing if the table 
	is not a view */
/*
cerr << "DEBUG (manage_parent):  "
	<< "Entering manage_parent"<<endl;
*/
	if(is_view && (views!=NULL) )
	{
		/* release parent view only if this is the only
		copy holding it */
		int number_copies=views->count(parent_table.table);
		multiset<int>::iterator vptr;
		if(number_copies<=0)
		{
/*
cerr << "DEBUG (manage_parent):  "
 << "No copies set of this view.  This should not happen if this is working right." <<endl;
*/
			parent_table.database=dbINVALID;
			parent_table.table=dbINVALID;
			parent_table.field=dbINVALID;
			parent_table.record=dbINVALID;
		}
		else
		{
			/* Assume this will always work if we get here.
			count should not return a positive number and then
			fail here. */
			vptr=views->find(parent_table.table);
			views->erase(vptr);
/*
cerr << "DEBUG (manage_parent):  "
	<< "number_copies of parent view="<<number_copies<<endl;
*/
			if(number_copies==1)
			{
//cerr << "Calling dbfree"<<endl;
				int testfree=dbfree(parent_table);
				/* We don't make this a fatal error.  
				Since it only effects the parent the 
				result may be usable, although odds are
				the caller will throw and exception if
				this occurs. */
				if(SEISPP_verbose && (testfree!=0))
				{
					cerr << "DatascpeHandle:  WARNING "
					 << "dbfree failed in automatic view"
					 << " memory managment." <<endl
					 << "Downstream errors may cause abort."
					 << endl;
				}
				parent_table.database=dbINVALID;
				parent_table.table=dbINVALID;
				parent_table.field=dbINVALID;
				parent_table.record=dbINVALID;
			}
		}
	}
}
} // End SEISPP namespace declaration
