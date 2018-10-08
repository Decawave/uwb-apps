function [mu,sigma] = clkcal()

tcp = tcpclient('127.0.0.1', 19021);

ntimes = 3000
idx = [];
utime =[];
ccp =[];
skew =[];
data = [];

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
                if (isfield(line,'utime') && isfield(line,'clkcal') )
                        utime(end+1) = line.utime / 1e6;
                        ccp(end+1,:) = line.clkcal;
                        skew(end+1) = typecast(uint64(line.skew),'double');
                end
            end
        end
        data = data(idx(i+1):end);
     end
     
     [~,m] = size(utime);
     
     if (mod(j,2) == 0)
        subplot(221); plot(utime,skew);title('skew')
        subplot(222); plot(utime,ccp(:,2));title('delta')
     end
     
     if (mod(j,4) == 0)
            [mu,sigma,~,~] = normfit((skew - 1));
            subplot(223);histfit(skew,16,'normal');title(sprintf("mu=%f sigma=%f",mu+1,sigma))
     end
     if (mod(j,4) == 0)
            [mu,sigma,~,~] = normfit(ccp(:,2));
            subplot(224);histfit(ccp(:,2),16,'normal');title(sprintf("mu=%f sigma=%f",mu,sigma))
     end  
end
 


