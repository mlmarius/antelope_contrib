LIB = libttregions.a
DLIB=$(LIB:.a=$(DSUFFIX))

DATA=
DATADIR = 

ldlibs=$(DBLIBS)

SUBDIR=/contrib
include $(ANTELOPEMAKE)
DIRS=

OBJS=ttregions.o winding.o
$(LIB) : $(OBJS)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LORDER) $(OBJS) $(TSORT)
	$(RANLIB) $@

        
$(DLIB) : $(OBJS)
	$(RM) $@
	case `uname -s` in \
	    SunOS) $(CC) $(CONTRIBDLDFLAGS) -o $@ $(DLDLIBS) $(OBJS) ;; \
	    Linux) $(DLD) $(CONTRIBDLDFLAGS) -o $@ $(DLDLIBS) -Wl,-init,_ttregions_init $(OBJS) ;; \
	    Darwin) $(DLD) $(CONTRIBDLDFLAGS) -o $@ $(DLDLIBS) $(OBJS) ;; \
	    *)  echo dynamic $(DLIB) not created: System type `uname -s` not recognized. ;;\
	esac

# $Id$ 
