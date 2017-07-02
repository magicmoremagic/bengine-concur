tool 'tools-gfx' {
   app 'concur' {
      icon 'icon/bengine-warm.ico',
      src 'src-concur/*.cpp',
      link_project {
         'core',
         'core-id-with-names',
         'util',
         'util-fs',
         'cli',
         'ctable',
         'gfx'
      }
   }
}
