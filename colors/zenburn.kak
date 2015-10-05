# zenburn theme

# first we define the zenburn colors as faces
face zenbackground   default,rgb:3f3f3f
face zenstatus       rgb:efdcbc,rgb:2a2a2a
face zencursor       default,rgb:7f9f7f
face zeninfo         rgb:cc9393,rgb:2a2a2a
face zenmenubg       rgb:7f9f7f,rgb:4a4a4a
face zenmenufg       rgb:8cd0d3,rgb:4a4a4a
face zentext         rgb:efefef
face zenkeyword      rgb:f0dfaf+b
face zenstorageClass rgb:c3bf9f+b
face zennumber       rgb:8cd0d3
face zencomment      rgb:7f9f7f
face zenconstant     rgb:dca3a3+b
face zenspecial      rgb:cfbfaf
face zenfunction     rgb:efef8f
face zenstatement    rgb:e3ceab
face zenidentifier   rgb:efdcbc
face zentype         rgb:dfdfbf
face zenstring       rgb:cc9393
face zenexception    rgb:c3bf9f+b
face zenmatching     rgb:3f3f3f,rgb:8cd0d3

# then we map them to code
face value zenconstant
face type zentype
face identifier zenidentifier
face string zenstring
face error zenexception
face keyword zenkeyword
face operator zenfunction
face attribute zenstatement
face comment zencomment
face meta zenspecial

# and markup
face title zenkeyword
face header zenconstant
face bold zenstorageClass
face italic zenfunction
face mono zennumber
face block zenstatement
face link zenstring
face bullet zenidentifier
face list zentype

# and built in faces
face Default zenbackground
face StatusLine zenstatus
face StatusCursor zencursor
face Information zeninfo
face MenuBackground zenmenubg
face MenuForeground zenmenufg
face MenuInfo rgb:cc9393
