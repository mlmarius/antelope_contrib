#
#   program needs:
#    make process station db table
#    start at directory head
#    miniseed2days BH and LH data
#    miniseed2days VH, UH, and SOH data
#    miniseed2db into one db
#    attach dbmaster, dbops, and idserver
#    verify if start and endtimes match deployment table
#    rtdaily_return
#    fill gaps from rtsystem
#    obsip2orb
#    make directory
#
#   DONE idservers dbpath locking 
#   DONE net-sta chec check
#   DONE check to make sure correct channels are in file
#
#   gap definition - time is first missing sample time
#                  - time + tgap is time of next sample
#                  - need to request gap.time to gap.time + tgap - 0.5*(1/samprate)
#   need to put dbmaster to $dbname station descriptor file before db2sync
#   check for overlaps
#
#
    require "getopts.pl" ;
    use strict ;
    use Datascope ;
    use archive;
    use timeslice ;
    use orb ;
    
    our ($pgm,$host);
    our ($opt_v,$opt_V,$opt_m,$opt_n,$opt_p);
    
{    #  Main program

    my ($usage,$cmd,$subject,$verbose,$debug,$Pf,$problems);
    my ($stime,$sta,$now,$dbops);
    my (@db,@dbdeploy);
    my ($row,$nrows,$rtsta);
    my (%pf);

    $pgm = $0 ; 
    $pgm =~ s".*/"" ;
    elog_init($pgm, @ARGV);
    $cmd = "\n$0 @ARGV" ;
    
    if (  ! &Getopts('vVnm:p:') || @ARGV != 1 ) { 
        $usage  =  "\n\n\nUsage: $0  \n	[-v] [-V] [-n] \n" ;
        $usage .=  "	[-p pf] [-m mail_to]  \n" ;
        $usage .=  "	dbops\n\n"  ; 
        
        elog_notify($cmd) ; 
        elog_die ( $usage ) ; 
    }
    
    &savemail() if $opt_m ; 
    elog_notify($cmd) ; 
    $stime = strydtime(now());
    chop ($host = `uname -n` ) ;
    elog_notify ("\nstarting execution on	$host	$stime\n\n");
    
    $dbops = $ARGV[0];

    $Pf         = $opt_p || $pgm ;
        
    %pf = getparam($Pf);

    $opt_v      = defined($opt_V) ? $opt_V : $opt_v ;    
    $verbose    = $opt_v;
    $debug      = $opt_V;
    
    if (system_check(0)) {
        $subject = "Problems - $pgm $host	Ran out of system resources";
        &sendmail($subject, $opt_m) if $opt_m ; 
        elog_die("\n$subject");
    }
    $problems = 0;

#   subset for unprocessed data

    $now = now();

    @db = dbopen($dbops,"r+");
    @dbdeploy = dblookup(@db,0,"deployment",0,0);
    @dbdeploy = dbsubset(@dbdeploy,"decert_time < $now  && snet =~ /TA/");
    @dbdeploy = dbsort(@dbdeploy,"sta");
    
    $nrows = dbquery(@dbdeploy,"dbRECORD_COUNT");

#
#  process all new stations
#

    for ($row = 0; $row<$nrows; $row++) {
        $stime = strydtime(now());
#        elog_notify ("\nstarting processing station $sta\n\n");
     
        $dbdeploy[3] = $row;
        $sta = dbgetv(@dbdeploy,"sta");
#
#  Build rt station wfdisc 
#

        $rtsta = "$pf{rt_sta_dir}/$sta";
        
        if (-e "$rtsta.wfdisc") {
            elog_notify ("\n$rtsta.wfdisc already exists!	Skipping $sta");
            next;
        }
        $cmd  = "miniseed2db ";
        $cmd .= "-v " if $opt_V;
        $cmd .= "$pf{msd2db_pat} $rtsta ";
        $cmd .= "> /dev/null 2>&1 " unless $opt_V ;
        $cmd =~ s/STA/$sta/;
            
        if  ( ! $opt_n ) {
            elog_notify("\n$cmd");        
            $problems = run($cmd,$problems) ;
        } else {
            elog_notify("\nskipping $cmd") ;
        } 
            
        unlink("$rtsta");
        unlink("$rtsta.snetsta");
        unlink("$rtsta.schanloc");
        unlink("$rtsta.lastid");
            
        &cssdescriptor ($rtsta,$pf{dbpath},$pf{dblocks},$pf{dbidserver}) ;

    }
        
    $stime = strydtime(now());
    elog_notify ("completed successfully	$stime\n\n");

    $subject = sprintf("Success  $pgm  $host");
    elog_notify ($subject);
    &sendmail ( $subject, $opt_m ) if $opt_m ;
  
    exit(0);
}

sub getparam { # %pf = getparam($Pf);
    my ($Pf) = @_ ;
    my ($subject);
    my (%pf) ;
    
    $pf{balerdirbase}		= pfget( $Pf, "balerdirbase" );
    $pf{archivebase}		= pfget( $Pf, "archivebase" );
    $pf{bhdata_dir}			= pfget( $Pf, "bhdata_dir" );
    $pf{sohdata_dir}		= pfget( $Pf, "sohdata_dir" );
    
    $pf{dbops}     		    = pfget( $Pf, "dbops" );

    $pf{dbpath}     		= pfget( $Pf, "dbpath" );
    $pf{dbidserver} 		= pfget( $Pf, "dbidserver" );
    $pf{dblocks}    		= pfget( $Pf, "dblocks" );
    
    $pf{wfclean}     		= pfget( $Pf, "wfclean" );
    $pf{msd2db_pat} 		= pfget( $Pf, "msd2db_pat" );
    $pf{rt_sta_dir}    		= pfget( $Pf, "rt_sta_dir" );
    
    $pf{deploy_mail} 		= pfget( $Pf, "deploy_mail" );
    $pf{prob_mail}    		= pfget( $Pf, "prob_mail" );
    
    if ($opt_V) {
        elog_notify("\nbalerdirbase     $pf{balerdirbase}");
        elog_notify("archivebase      $pf{archivebase}");
        elog_notify("bhdata_dir       $pf{bhdata_dir}");
        elog_notify("sohdata_dir      $pf{sohdata_dir}");
        elog_notify("dbops            $pf{dbops}" );
        elog_notify("dbpath           $pf{dbpath}" );
        elog_notify("dbidserver       $pf{dbidserver}" );
        elog_notify("dblocks          $pf{dblocks}\n\n" );
        elog_notify("wfclean          $pf{wfclean}" );
        elog_notify("msd2db_pat       $pf{msd2db_pat}" );
        elog_notify("rt_sta_dir       $pf{rt_sta_dir}\n\n" );
        elog_notify("deploy_mail      $pf{deploy_mail}" );
        elog_notify("prob_mail        $pf{prob_mail}\n\n" );
    }
    
    makedir $pf{rt_sta_dir} if (! -d $pf{rt_sta_dir});
        
    return (%pf) ;
}

 