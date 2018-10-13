tcp = tcpclient('127.0.0.1', 19021);

ntimes = 3000
idx = [];
data= [];
utime =[];
tof =[];
fp_idx =[];
range = [];

for j=1:ntimes
    
    data = [data, read(tcp)];
    str = char(data);
    idx = find(str == 13);
    
    while (length(idx) < 16)
        data = [data, read(tcp)];
        str = char(data);
        idx = find(str == 13);
    end  
    
     for i=1:(length(idx) - 2)
        line = str((idx(i)+2):idx(i+1));
        if (line(1) == '{')
            line = jsondecode(line);
            if (isstruct(line)) 
                if (isfield(line,'utime') && isfield(line,'tof') && isfield(line,'range'))
                    utime(end+1) = line.utime;
                    tof(end+1) = typecast(uint32(line.tof),'single');
                    range(end+1) = typecast(uint32(line.range),'single');
                end
            end
        end
        data = data(idx(i+1):end);
     end
     
     [~,m] = size(utime);
     
     if (mod(j,4) == 0)
        pause(0.001)
        refreshdata;
     end
     if (mod(j,4) == 0)
            [mu,sigma,~,~] = normfit(range);
            subplot(212);histfit(range,16,'normal');title(sprintf("mu=%f sigma=%f",mu,sigma))
     end
      if (mod(j,64) == 0)
            [mu,sigma,~,~] = normfit(tof);
            subplot(211);title(sprintf("tof mu=%f tof sigma=%f (dwt unit)",mu,sigma))
     end
     
    if (j==10)
        linkdata off
        subplot(211);
        plot(utime,range,'b');
        xlabel('utime')
        ylabel('range(m)')
        linkdata on
    end
    
end
 


