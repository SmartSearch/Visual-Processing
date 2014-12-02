g++ ../../common/curl_helper.c bgfg_ait.cpp ../../common/time_funcs.c video_seq.cpp main.cpp simple_crowd_analysis.cpp -lcurl `pkg-config --cflags --libs opencv` -I ../../common -o ../bin/visual_analysis
echo .
echo "visual_analysis" executable should now be available at ../bin
echo If you get errors about missing curl.h, run  sudo apt-get install libcurl4-openssl-dev
echo .
