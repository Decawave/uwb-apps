tcp = tcpclient('127.0.0.1', 19021);

ntimes = 3000
idx = [];
data= [];
utime =[];
Tp =[];
fp_idx =[];
range = [];

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
                if (isfield(line,'utime'))
                    utime(end+1) = line.utime;
                    Tp(end+1) = line.Tp/4.0;
                    fp_idx(end+1) = line.fp_idx;
                    range(end+1) = line.Tp .* 299792458 * (1.0/499.2e6/128.0)/4.0;
                end
            end
        end
        data = data(idx(i+1):end);
     end
     
     [~,m] = size(utime);
     
     if (mod(j,32) == 0)
        pause(0.001)
        refreshdata;
     end
     if (mod(j,32) == 0)
            [mu,sigma,~,~] = normfit(range);
            subplot(212);histfit(range,16,'normal');title(sprintf("mu=%f sigma=%f",mu,sigma))
     end
     
    if (j==10)
        linkdata off
        subplot(211);
        plot(utime,range,'b-.o');
        xlabel('utime')
        ylabel('range(m)')
        linkdata on
    end    
end
 
if (m > 64)
            [mu,sigma,~,~] = normfit(range);
            subplot(212);histfit(range,16,'normal');title(sprintf("mu=%f sigma=%f",mu,sigma))
end



