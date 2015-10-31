#!/bin/bash

cd performance
rm dma_thru/d1*
rm dma_thru_read dma_thru_write

rm dma_lat/dl*
rm dma_lat_read

rm pdma_thru/pd*
rm pdma_thru_*_read pdma_thru_*_write

rm obwin_lat/ol*
rm obwin_lat_read

rm obwin_thru/o1R*
rm obwin_thru/o1W*
rm obwin_thru/o8R*
rm obwin_thru/o8W*
rm obwin_thru_read obwin_thru_write

rm msg_thru/m*
rm msg_thru_tx

rm msg_lat/m*
rm msg_lat_rx

cd ..
rm start_source start_target
