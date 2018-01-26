# ngx_http_small_file_merge

在移动时代，使用hls来播放小视频越来越多。相对于大文件，小文件的发送效率低下。特别在网络较差的环境中，用户体验更加不好。在这种情况下，在服务器端对小文件来进行按块发送，对提高网络的利用率有较大的提升。

播放串请求格式如下:
1. http://www.xxx.com/data/ts/?file_list=0_5840_v01_mp4.ts:5840_12400_v01_mp4.ts:12400_22400_v01_mp4.ts:22400_31400_v01_mp4.ts

2. http://www.xxx.com/data/ts/0_5840_v01_mp4.ts?file_list=0_5840_v01_mp4.ts:5840_12400_v01_mp4.ts:12400_22400_v01_mp4.ts:22400_31400_v01_mp4.ts

1.配置
     location ~ /data {
            file_merge on; 
            file_merge_number 5;   
        }   
        
2. 指令
      file_merge on | off
      default: file_merge off
      context: http, server, location
      说明:开启按块发送功能;
      
      
      file_merge_number number;
      default:file_merge_number 10;
      context: http, server, location
      说明:设置一次最多发送多少个文件;
      

   


