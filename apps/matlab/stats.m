tcp = tcpclient('127.0.0.1', 19021);

ntimes = 12000
idx = [];
data= [];
utime =[];
tof =[];
fp_idx =[];
range = [];
skew = [];
skew_wcs = [];
utime_wcs = [];
wcs = [];

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
                if (isfield(line,'utime') && isfield(line,'tof') && isfield(line,'range') && isfield(line,'skew') )
                    utime(end+1) = line.utime;
                    tof(end+1) = typecast(uint32(line.tof),'single');
                    range(end+1) = typecast(uint32(line.range),'single');
                    skew(end+1) = typecast(uint32(line.skew),'single') * 1e6;
                end
                if (isfield(line,'utime') && isfield(line,'wcs'))
                    utime_wcs(end+1) = line.utime;
                    wcs(end+1,:) = line.wcs;
                    skew_wcs(end+1) = typecast(uint64(line.skew),'double') * 1e6;
                end
            end
        end
        data = data(idx(i+1):end);
     end
     nwin = 1000;
     [~,m] = size(utime);
     
     if (mod(j,4) == 0)
        pause(0.04)
        refreshdata;
     end
     if (mod(j,4) == 0)
            [mu,sigma,~,~] = normfit(range);
            ax = subplot(211);
            yyaxis(ax,'right')
            plot(utime,skew,utime_wcs,skew_wcs,'o-m');
            yyaxis(ax,'left')
            plot(utime,range);
%            plot(utime_wcs,skew_wcs)
            title(sprintf("tof mu=%f tof sigma=%f (dwt unit)",mu,sigma))
     end
     if (mod(j,4) == 0 && length(range) > nwin)
            [mu,sigma,~,~] = normfit(range((end-nwin):end));
            subplot(234);histfit(range((end-nwin):end),16,'normal');title(sprintf("range mu=%f (m) sigma=%f (m)",mu,sigma))
     end
     if (mod(j,4) == 0 && length(skew) > nwin)
            [mu,sigma,~,~] = normfit(skew((end-nwin):end));
            subplot(235);histfit(skew((end-nwin):end),16,'normal');title(sprintf("skew mu=%f (usec) sigma=%f (usec)",mu,sigma))
     end
     if (mod(j,32) == 0 )
            [mu,sigma,~,~] = normfit(skew_wcs);
            subplot(236);histfit(skew_wcs,16,'normal');title(sprintf("wcs mu=%f (usec) sigma=%f (usec)",mu,sigma))
     end

end
 


