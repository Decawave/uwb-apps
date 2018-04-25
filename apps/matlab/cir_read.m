tcp = tcpclient('127.0.0.1', 19021);
pause(1)
      
ntime = 3000
idx =[];
data=[];
fp_idx =[];
ir = [];
ir_current =[];
fp_idx_current =[];
t =[];
nwin = 64;


for j=1:ntime
   
    data = [data, read(tcp)];
    str = char(data);
    idx = find(str == 13);
    
    while (length(idx) < 4)
        data = [data, read(tcp)];
        str = char(data);
        idx = find(str == 13);
    end    
            
    for i=1:length(idx)-2
        line = str((idx(i)+2):idx(i+1));
        if (line(1) == '{')
            line = jsondecode(line);
            if (isstruct(line)) 
                if (isfield(line,'utime') && isfield(line,'cir'))
                    ir(end+1,:) = complex(line.cir.real,line.cir.imag)';
                    fp_idx(end+1) = line.cir.fp_idx;
                end
            end
        end
        data = data(idx(i+1):end);
    end
    
    pause(0.01)
    refreshdata;
    
    [n,m] = size(ir);
   
     if(n > nwin)
        t = (1:m);
        abs_ir_current = abs(ir(end,:));
        real_ir_current = real(ir(end,:));
        imag_ir_current = imag(ir(end,:));
       
        fp_idx_current = fp_idx(end);
        
 %       subplot(211);
         [upper,lower]=envelope(abs(ir((end-nwin):end,:))',8,'peak');
 
        plot(t,[abs_ir_current;mean(upper',1);mean(lower',1)]);
        hold on
        plot(t,[abs_ir_current],'+');
        hold off
         
        xlabel('utime')
        ylabel('range(m)')
        
%        subplot(212);
%        p = angle(ir(end,:));
%        plot(t,p);
%        xlabel('utime')
%        ylabel('range(m)')
        
        pause(0.01)
        refreshdata;
       
     end
end





