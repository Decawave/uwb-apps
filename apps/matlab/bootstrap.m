function bootstrap(varargin)

here=pwd;
root = getenv('MYNEWT_DW1000_APPS_ROOT');

if isempty(root)
    root = which('bootstrap');
    switch(computer)
        case {'MACI64','GLNXA64','x86_64-apple-darwin14.5.0'}    
        root=root(1:strfind(root,'/matlab'));
        case {'PCWIN64'}    
        root=root(1:strfind(root,'\matlab'));
    end  
    setenv('MYNEWT_DW1000_APPS_ROOT',root);
end

cleanup = onCleanup(@() cd(here));
cd([getenv('MYNEWT_DW1000_APPS_ROOT'),'/matlab']);

if isempty(varargin)
    cmd = 'all';
else
    cmd = varargin{1};
end

if ~exist('SlotIndex_mex','file') || strcmp(cmd,'all') || strcmp(cmd,'SlotIndex_mex')
    mex -outdir . -O CFLAGS="\$CFLAGS -std=c99" -DMEX -I"./" ...
        SlotIndex_mex.cpp ...
        slots.c
end



