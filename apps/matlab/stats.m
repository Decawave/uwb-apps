tcp = tcpclient('127.0.0.1', 19021);

ntimes = 3000
idx = [];
data= [];
utime =[];
tof =[];
fp_idx =[];
range = [];
skew = [];
skew_wcs = [];
utime_wcs = [];
ccp = [];

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
                if (isfield(line,'utime') && isfield(line,'tof') && isfield(line,'range') && isfield(line,'dkew'))
                    utime(end+1) = line.utime;
                    tof(end+1) = typecast(uint32(line.tof),'single');
                    range(end+1) = typecast(uint32(line.range),'single');
                    skew(end+1) = typecast(uint32(line.skew),'single') * 1e6;
                end
                if (isfield(line,'utime') && isfield(line,'wcs') )
                        utime_wcs(end+1) = line.utime;
                        ccp(end+1,:) = line.wcs;
                        skew_wcs(end+1) = typecast(uint64(line.skew),'double') * 1e6;
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
     if (mod(j,16) == 0)
            [mu,sigma,~,~] = normfit(range);
            subplot(211);
            yyaxis right
            plot(utime,skew, utime_wcs,skew_wcs,'r');
            yyaxis left
            plot(utime,range);
            title(sprintf("tof mu=%f tof sigma=%f (dwt unit)",mu,sigma))
     end
     if (mod(j,4) == 0)
            [mu,sigma,~,~] = normfit(range);
            subplot(234);histfit(range,16,'normal');title(sprintf("range mu=%f (usec) sigma=%f (usec)",mu,sigma))
     end
     if (mod(j,4) == 0)
            [mu,sigma,~,~] = normfit(skew);
            subplot(235);histfit(skew,16,'normal');title(sprintf("skew mu=%f (usec) sigma=%f (usec)",mu,sigma))
     end
     if (mod(j,32) == 0)
            [mu,sigma,~,~] = normfit(skew_wcs);
            subplot(236);histfit(skew_wcs,16,'normal');title(sprintf("wcs mu=%f (usec) sigma=%f (usec)",mu,sigma))
     end
%     if (mod(j,64) == 0)
%            [mu,sigma,~,~] = normfit(skew * 1e6);
%            subplot(222);title(sprintf("mu=%f (usec) sigma=%f (usec)",mu,sigma))
%     end

%    if (j==10)
%        linkdata off
%        subplot(211);plot(utime,skew,'b', utime_wcs,skew_wcs,'r');
%        xlabel('utime')
%        ylabel('range(m)')
%        linkdata on
%    end
    
end
 


