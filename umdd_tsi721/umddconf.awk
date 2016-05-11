BEGIN{ OFS=FS; FS="//"; }
{ if(length($1) > 0) { A[i++] = $1;} }
END{
  FS=OFS;

  print "isol";
  for(i = 1; i < 5; i++) { printf("set cpu%d ? -1\n", i); }
  print "////";

  th=-1;
  n=i;
  for(i=0; i<n; i++) {
    $0 = A[i];
    if(tolower($1) == "end") { break; }
    if(tolower($1) == "mport") { print "mport " $2; print "////"; continue; }
    if(tolower($1) == "chan") {
      th++;
      channo = $2; printf("set chan%d %d\n", th, channo);
      buf    = strtonum($3); printf("set buf%d %x\n",  th, buf);
      sts    = strtonum($4); printf("set sts%d %x\n",  th, sts);
      isol   = $5;
      if(isol == "") { cpu = "-1"; } else { cpu = "$cpu" isol; }
      while(( getline line < "scripts/umdd.tmpl") > 0 ) {
         gsub(/%th%/, th, line);
         gsub(/%cpu%/, cpu, line);
         print line;
      }
      close("scripts/umdd.tmpl");
      print "////";
    }
  }
}
