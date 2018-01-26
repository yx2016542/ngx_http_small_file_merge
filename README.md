# ngx_http_small_file_merge
在当今时代，视频成为最重要的娱乐媒介。其中在当前的视频文件中，使用hls类似的小文件越来越多，但是小文件相对大文件来说, 发送效率较低，特别是针对网络较差的情况，不断的发送小文件请求，效率较低，本项目针对这种情况，做了相应的优化，对小文件成块发送来提高发送效率.

播放串请求格式如下:

http://www.xxx.com/data/ts/?file_list=0_5840_v01_mp4.ts:5840_12400_v01_mp4.ts:12400_22400_v01_mp4.ts:22400_31400_v01_mp4.ts

http://www.xxx.com/data/ts/0_5840_v01_mp4.ts?file_list=0_5840_v01_mp4.ts:5840_12400_v01_mp4.ts:12400_22400_v01_mp4.ts:22400_31400_v01_mp4.ts

1.配置

 location ~ /data {
 
        file_merge on; 
        file_merge_number 5;   
    }   
   
2.指令

file_merge on | off

default: file_merge off

context: http, server, location

     说明:开启按块发送功能;

  file_merge_number number;
  
  default:file_merge_number 10;
  
  context: http, server, location
     说明:设置一次最多发送多少个文件;    
