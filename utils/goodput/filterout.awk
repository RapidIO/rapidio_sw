#!/usr/bin/awk -f
BEGIN{ FS=","; cnt=1; }
/csv/ && /STS size/ {
  if(s==0) { # extract ONE header
    tp=$NF; # OpenOffice is F**ng braindead, must reorder/dupe columns
    s=1;
    print $0 ",-EMPTY-,DMA Write Size (dec),DMA Write Size (log2)," tp;
  }
}
/csv/ && !/STS size/ {
  if(acc_size[$4]++) { # select every SECOND measurement
    cell=sprintf("D%d", ++cnt);
    tp=$NF; # OpenOffice is F**ng braindead, must reorder/dupe columns
    print $0 ",,=hex2dec(" cell "),=log(hex2dec(" cell ");2)," tp;
  }
}
