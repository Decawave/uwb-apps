function [mu,sigma] = stats(src_address)

tcp = tcpclient('127.0.0.1', 19021);

ntimes = 3000
idx = [];
data= [];
tof =[];
fp_idx =[];
range = [];
utime=[];
azimuth =[];

for j=1:ntimes
    
    data = [data, read(tcp)];
    str = char(data);
    idx = find(str == 13);
    
    while (length(idx) < 4)
        data = [data, read(tcp, 1024)];
        str = char(data);
        idx = find(str == 13);
    end  
    
     for i=1:(length(idx) - 2)
        line = str((idx(i)+2):idx(i+1));
        if (line(1) == '{')
            line = jsondecode(line);
            if (isstruct(line)) 
                if (isfield(line,'utime') && isfield(line,'range') && isfield(line,'azimuth'))
                        utime(end+1) = line.utime;
                        range(end+1) = typecast(uint32(line.range),'single');
                        azimuth(end+1) = typecast(uint32(line.azimuth),'single') * 180/pi;
                end
            end
        end
        data = data(idx(i+1):end);
     end
     
     [~,m] = size(utime);
     
     if (mod(j,4) == 0)
        subplot(221); plot(utime,range);
        subplot(222); plot(utime,azimuth);
       % pause(0.001)
       % refreshdata;
     end
     
     if (mod(j,16) == 0)
            [mu,sigma,~,~] = normfit(range);
            subplot(223);histfit(range,16,'normal');title(sprintf("mu=%f sigma=%f",mu,sigma))
     end
     if (mod(j,16) == 0)
            [mu,sigma,~,~] = normfit(azimuth);
            subplot(224);histfit(azimuth,16,'normal');title(sprintf("mu=%f sigma=%f",mu,sigma))
     end  
end
 


